#include "Metal.h"

#if ATLAS_METAL && defined( __OBJC__ )

#include <cstring>

#import <Foundation/Foundation.h>

#include "Sheet.h"
#include "Context.h"
#include "Shaders.h"

#if !__has_feature( objc_arc )
#error "The Atlas Metal port must be built with automatic reference counting"
#endif

static_assert( sizeof( CInstance ) == 80, "The instance layout must match the shader" );

static id< MTLLibrary > BuildLibrary( id< MTLDevice > Device, const std::string& Source ) {
    NSString* Text = [NSString stringWithUTF8String:Source.c_str( )];
    if ( !Text )
        return nil;

    NSError* Fault = nil;
    id< MTLLibrary > Library = [Device newLibraryWithSource:Text options:nil error:&Fault];

    if ( !Library && Fault )
        Context->Report( [[Fault localizedDescription] UTF8String] );

    return Library;
}

CMetal::~CMetal( ) {
    Destroy( );
}

bool CMetal::Create( id< MTLDevice > Device, MTLPixelFormat Kind, int Frames, MTLPixelFormat DepthKind, int Samples ) {
    Destroy( );

    if ( !Device || Kind == MTLPixelFormatInvalid || Frames <= 0 || Samples <= 0 )
        return false;

    Hardware = Device;

    Format = Kind;
    DepthFormat = DepthKind;

    SampleCount = Samples;
    VertexLibrary = BuildLibrary( Hardware, Shaders->Vertex( ShaderMetal ) );

    if ( !VertexLibrary ) {
        Destroy( );
        return false;
    }

    VertexStage = [VertexLibrary newFunctionWithName:@"MainVertex"];
    if ( !VertexStage ) {
        Context->Report( "Atlas could not find the Metal vertex stage" );

        Destroy( );
        return false;
    }

    MTLSamplerDescriptor* Filtering = [[MTLSamplerDescriptor alloc] init];

    Filtering.minFilter = MTLSamplerMinMagFilterLinear;
    Filtering.magFilter = MTLSamplerMinMagFilterLinear;

    Filtering.mipFilter = MTLSamplerMipFilterLinear;
    Filtering.sAddressMode = MTLSamplerAddressModeClampToEdge;

    Filtering.tAddressMode = MTLSamplerAddressModeClampToEdge;
    Filtering.rAddressMode = MTLSamplerAddressModeClampToEdge;

    Sampler = [Hardware newSamplerStateWithDescriptor:Filtering];

    MTLDepthStencilDescriptor* Ignoring = [[MTLDepthStencilDescriptor alloc] init];

    Ignoring.depthCompareFunction = MTLCompareFunctionAlways;
    Ignoring.depthWriteEnabled = NO;

    Testing = [Hardware newDepthStencilStateWithDescriptor:Ignoring];

    if ( !Sampler || !Testing ) {
        Destroy( );
        return false;
    }

    FrameCount = Frames;
    Cursor = 0;

    Rings.resize( ( size_t )Frames );

    if ( !Pipe( 0 ) ) {
        Destroy( );
        return false;
    }

    for ( CMetalFrame& Ring : Rings ) {
        if ( !Reserve( Ring, 16384 ) ) {
            Destroy( );
            return false;
        }
    }

    return true;
}

void CMetal::Destroy( ) {
    Images.clear( );
    Pipelines.clear( );

    Rings.clear( );
    SheetTexture = nil;

    Sampler = nil;
    Testing = nil;

    VertexStage = nil;
    VertexLibrary = nil;

    Hardware = nil;

    Format = MTLPixelFormatInvalid;
    DepthFormat = MTLPixelFormatInvalid;

    SampleCount = 1;
    SheetVersion = 0;

    SheetDepth = 0;
    NextImage = 1;

    FrameCount = 0;
    Cursor = 0;
}

id< MTLRenderPipelineState > CMetal::Pipe( unsigned int Effect ) {
    size_t Wanted = ( size_t )Effect + 1;

    if ( Pipelines.size( ) < Wanted )
        Pipelines.resize( Wanted );

    if ( Pipelines[ Effect ] )
        return Pipelines[ Effect ];

    if ( !Hardware || !VertexStage )
        return nil;

    id< MTLLibrary > Library = BuildLibrary( Hardware, Shaders->Pixel( Effect, ShaderMetal ) );
    id< MTLFunction > FragmentStage = Library ? [Library newFunctionWithName:@"MainFragment"] : nil;

    if ( !FragmentStage ) {
        Context->Report( "Atlas could not build a Metal fragment stage" );

        if ( Effect == 0 )
            return nil;

        id< MTLRenderPipelineState > Spare = Pipe( 0 );

        Pipelines[ Effect ] = Spare;
        return Spare;
    }

    MTLRenderPipelineDescriptor* Recipe = [[MTLRenderPipelineDescriptor alloc] init];

    Recipe.vertexFunction = VertexStage;
    Recipe.fragmentFunction = FragmentStage;

    Recipe.rasterSampleCount = ( NSUInteger )SampleCount;
    Recipe.depthAttachmentPixelFormat = DepthFormat;

    if ( DepthFormat == MTLPixelFormatDepth32Float_Stencil8 )
        Recipe.stencilAttachmentPixelFormat = DepthFormat;

    Recipe.colorAttachments[ 0 ].pixelFormat = Format;
    Recipe.colorAttachments[ 0 ].blendingEnabled = YES;

    Recipe.colorAttachments[ 0 ].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    Recipe.colorAttachments[ 0 ].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    Recipe.colorAttachments[ 0 ].rgbBlendOperation = MTLBlendOperationAdd;
    Recipe.colorAttachments[ 0 ].sourceAlphaBlendFactor = MTLBlendFactorOne;

    Recipe.colorAttachments[ 0 ].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    Recipe.colorAttachments[ 0 ].alphaBlendOperation = MTLBlendOperationAdd;

    Recipe.colorAttachments[ 0 ].writeMask = MTLColorWriteMaskAll;

    NSError* Fault = nil;
    id< MTLRenderPipelineState > Built = [Hardware newRenderPipelineStateWithDescriptor:Recipe error:&Fault];

    if ( !Built ) {
        if ( Fault )
            Context->Report( [[Fault localizedDescription] UTF8String] );

        if ( Effect == 0 )
            return nil;

        id< MTLRenderPipelineState > Spare = Pipe( 0 );

        Pipelines[ Effect ] = Spare;
        return Spare;
    }

    Pipelines[ Effect ] = Built;
    return Built;
}

bool CMetal::Reserve( CMetalFrame& Ring, int Count ) {
    if ( Count <= Ring.Capacity )
        return true;

    int Goal = Ring.Capacity > 0 ? Ring.Capacity : 16384;
    while ( Goal < Count )
        Goal *= 2;

    Ring.Storage = nil;
    Ring.Capacity = 0;

    Ring.Storage = [Hardware newBufferWithLength:( NSUInteger )Goal * sizeof( CInstance ) options:MTLResourceStorageModeShared];

    if ( !Ring.Storage ) {
        Context->Report( "Atlas could not grow the Metal instance buffer" );
        return false;
    }

    Ring.Capacity = Goal;
    return true;
}

id< MTLTexture > CMetal::CreateTexture( int Width, int Height, MTLPixelFormat Kind ) {
    if ( !Hardware )
        return nil;

    MTLTextureDescriptor* Description = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Kind width:( NSUInteger )Width height:( NSUInteger )Height mipmapped:NO];

    Description.usage = MTLTextureUsageShaderRead;
    Description.storageMode = MTLStorageModeShared;

    return [Hardware newTextureWithDescriptor:Description];
}

void CMetal::Refresh( ) {
    if ( SheetVersion == Sheet->Version && SheetTexture && SheetDepth == Sheet->Depth( ) )
        return;

    if ( Sheet->Depth( ) <= 0 )
        return;

    id< MTLTexture > Fresh = CreateTexture( Sheet->Extent( ), Sheet->Depth( ), MTLPixelFormatR8Unorm );
    if ( !Fresh )
        return;

    MTLRegion Region = MTLRegionMake2D( 0, 0, ( NSUInteger )Sheet->Extent( ), ( NSUInteger )Sheet->Depth( ) );
    [Fresh replaceRegion:Region mipmapLevel:0 withBytes:Sheet->Data( ) bytesPerRow:( NSUInteger )Sheet->Extent( )];

    SheetTexture = Fresh;
    SheetVersion = Sheet->Version;

    SheetDepth = Sheet->Depth( );
}

unsigned long long CMetal::CreateImage( const unsigned char* Pixels, int Width, int Height ) {
    if ( !Hardware || !Pixels || Width <= 0 || Height <= 0 )
        return 0;

    if ( Width > 16384 || Height > 16384 ) {
        Context->Report( "Atlas rejected an image with unreasonable dimensions" );
        return 0;
    }

    CMetalImage Image;
    Image.Texture = CreateTexture( Width, Height, MTLPixelFormatRGBA8Unorm );

    if ( !Image.Texture ) {
        Context->Report( "Atlas could not create a Metal image" );
        return 0;
    }

    Image.Width = Width;
    Image.Height = Height;

    Image.Owned = true;

    MTLRegion Region = MTLRegionMake2D( 0, 0, ( NSUInteger )Width, ( NSUInteger )Height );
    [Image.Texture replaceRegion:Region mipmapLevel:0 withBytes:Pixels bytesPerRow:( NSUInteger )Width * 4];

    unsigned long long Handle = NextImage;
    NextImage++;

    Images[ Handle ] = Image;
    return Handle;
}

void CMetal::UpdateImage( unsigned long long Image, const unsigned char* Pixels ) {
    auto Entry = Images.find( Image );
    if ( Entry == Images.end( ) || !Entry->second.Owned || !Entry->second.Texture || !Pixels )
        return;

    MTLRegion Region = MTLRegionMake2D( 0, 0, ( NSUInteger )Entry->second.Width, ( NSUInteger )Entry->second.Height );
    [Entry->second.Texture replaceRegion:Region mipmapLevel:0 withBytes:Pixels bytesPerRow:( NSUInteger )Entry->second.Width * 4];
}

unsigned long long CMetal::AdoptImage( void* Native ) {
    if ( !Native )
        return 0;

    CMetalImage Image;

    Image.Texture = ( __bridge id< MTLTexture > )Native;
    Image.Owned = false;

    Image.Width = ( int )Image.Texture.width;
    Image.Height = ( int )Image.Texture.height;

    unsigned long long Handle = NextImage;
    NextImage++;

    Images[ Handle ] = Image;
    return Handle;
}

void CMetal::DestroyImage( unsigned long long Image ) {
    auto Entry = Images.find( Image );
    if ( Entry == Images.end( ) )
        return;

    Images.erase( Entry );
}

void CMetal::Bind( id< MTLRenderCommandEncoder > Orders, const CDrawData& Data ) {
    MTLViewport Viewport = { 0.0, 0.0, ( double )Data.Extent.Horizontal, ( double )Data.Extent.Vertical, 0.0, 1.0 };
    [Orders setViewport:Viewport];

    MTLScissorRect Scissor = { 0, 0, ( NSUInteger )Data.Extent.Horizontal, ( NSUInteger )Data.Extent.Vertical };
    [Orders setScissorRect:Scissor];

    [Orders setCullMode:MTLCullModeNone];
    [Orders setTriangleFillMode:MTLTriangleFillModeFill];

    [Orders setDepthStencilState:Testing];

    if ( !Rings.empty( ) && Rings[ Cursor ].Storage )
        [Orders setVertexBuffer:Rings[ Cursor ].Storage offset:0 atIndex:0];

    float Values[ 8 ] = { 2.0f / Data.Extent.Horizontal, -2.0f / Data.Extent.Vertical, -1.0f, 1.0f, 0.0f, Data.Moment, 0.0f, 0.0f };

    [Orders setFragmentBytes:Values length:sizeof( Values ) atIndex:0];
    [Orders setFragmentSamplerState:Sampler atIndex:0];
}

void CMetal::Render( const CDrawData& Data, void* Stream ) {
    id< MTLRenderCommandEncoder > Orders = ( __bridge id< MTLRenderCommandEncoder > )Stream;

    if ( !Hardware || !Orders || Rings.empty( ) || Data.Extent.Horizontal <= 0.0f || Data.Extent.Vertical <= 0.0f )
        return;

    Cursor = ( Cursor + 1 ) % FrameCount;
    CMetalFrame& Ring = Rings[ Cursor ];

    Refresh( );

    if ( Data.BatchCount <= 0 )
        return;

    if ( Data.Count > 0 && Data.Items ) {
        if ( !Reserve( Ring, Data.Count ) )
            return;

        memcpy( Ring.Storage.contents, Data.Items, ( size_t )Data.Count * sizeof( CInstance ) );
    }

    Bind( Orders, Data );

    for ( int Position = 0; Position < Data.BatchCount; Position++ ) {
        const CBatch& Batch = Data.Batches[ Position ];

        if ( Batch.Callback ) {
            Batch.Callback( ( __bridge void* )Orders, Batch.Detail );
            Bind( Orders, Data );

            continue;
        }

        if ( Batch.Count <= 0 )
            continue;

        id< MTLTexture > Bound = SheetTexture;

        if ( Batch.Image != 0 ) {
            auto Entry = Images.find( Batch.Image );
            Bound = Entry == Images.end( ) ? nil : Entry->second.Texture;
        }

        if ( !Bound )
            continue;

        id< MTLRenderPipelineState > Chosen = Pipe( Batch.Effect );
        if ( !Chosen )
            continue;

        float Values[ 8 ] = { 2.0f / Data.Extent.Horizontal, -2.0f / Data.Extent.Vertical, -1.0f, 1.0f, 0.0f, Data.Moment, 0.0f, 0.0f };
        ( ( unsigned int* )Values )[ 4 ] = ( unsigned int )Batch.Offset;

        [Orders setVertexBytes:Values length:sizeof( Values ) atIndex:1];
        [Orders setRenderPipelineState:Chosen];

        [Orders setFragmentTexture:Bound atIndex:0];
        [Orders drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:( NSUInteger )Batch.Count];
    }
}

#endif
