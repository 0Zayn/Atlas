#pragma once

#if !defined( ATLAS_METAL )
#if defined( __APPLE__ ) && __has_include( <Metal/Metal.h> )
#define ATLAS_METAL 1
#else
#define ATLAS_METAL 0
#endif
#endif

inline constexpr bool MetalReady = ATLAS_METAL != 0;

#if ATLAS_METAL && defined( __OBJC__ )

#include <memory>
#include <vector>
#include <unordered_map>

#import <Metal/Metal.h>

#include "Canvas.h"

class CMetalFrame {
public:
    id< MTLBuffer > Storage = nil;
    int Capacity = 0;
};

class CMetalImage {
public:
    id< MTLTexture > Texture = nil;

    int Width = 0;
    int Height = 0;

    bool Owned = false;
};

class CMetal : public CGraphics {
public:
    ~CMetal( );

    bool Create( id< MTLDevice > Device, MTLPixelFormat Kind, int Frames, MTLPixelFormat DepthKind = MTLPixelFormatInvalid, int Samples = 1 );
    void Destroy( ) override;

    void Render( const CDrawData& Data, void* Stream ) override;

    unsigned long long CreateImage( const unsigned char* Pixels, int Width, int Height ) override;
    void UpdateImage( unsigned long long Image, const unsigned char* Pixels ) override;

    unsigned long long AdoptImage( void* Native ) override;
    void DestroyImage( unsigned long long Image ) override;

private:
    bool Reserve( CMetalFrame& Ring, int Count );
    void Refresh( );

    void Bind( id< MTLRenderCommandEncoder > Orders, const CDrawData& Data );
    id< MTLRenderPipelineState > Pipe( unsigned int Effect );

    id< MTLTexture > CreateTexture( int Width, int Height, MTLPixelFormat Kind );

    id< MTLDevice > Hardware = nil;
    id< MTLLibrary > VertexLibrary = nil;

    id< MTLFunction > VertexStage = nil;
    std::vector< id< MTLRenderPipelineState > > Pipelines;

    id< MTLSamplerState > Sampler = nil;
    id< MTLDepthStencilState > Testing = nil;

    id< MTLTexture > SheetTexture = nil;

    std::vector< CMetalFrame > Rings;
    std::unordered_map< unsigned long long, CMetalImage > Images;

    unsigned long long NextImage = 1;
    unsigned int SheetVersion = 0;

    MTLPixelFormat Format = MTLPixelFormatInvalid;
    MTLPixelFormat DepthFormat = MTLPixelFormatInvalid;

    int SampleCount = 1;
    int SheetDepth = 0;

    int FrameCount = 0;
    int Cursor = 0;
};

inline auto Metal = std::make_unique< CMetal >( );

#endif
