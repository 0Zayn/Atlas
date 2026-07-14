#include "Shaders.h"

static const char* const HlslVertex = R"(
struct CInstance
{
    float4 Bounds;

    float4 Clip;
    float4 Source;

    uint ColorStart;
    uint ColorFinish;

    float Rounding;
    float Thickness;

    uint Flags;

    float Softness;
    float Inflate;

    uint Reserved;
};

StructuredBuffer< CInstance > Instances : register( t0 );

cbuffer CScreen : register( b0 )
{
    float4 Projection;
    uint Origin;
    float Moment;
};

struct CFragment
{
    float4 Position : SV_Position;
    float4 Color : COLOR0;

    float2 Local : TEXCOORD0;
    float2 Extent : TEXCOORD1;
    float2 Source : TEXCOORD2;

    nointerpolation float3 Shape : TEXCOORD3;
    nointerpolation uint Flags : TEXCOORD4;
    nointerpolation float4 Extra : TEXCOORD5;
};

float4 Unpack( uint Value )
{
    return float4( Value & 255, ( Value >> 8 ) & 255, ( Value >> 16 ) & 255, Value >> 24 ) / 255.0;
}

CFragment MainVertex( uint Vertex : SV_VertexID, uint Item : SV_InstanceID )
{
    CInstance Instance = Instances[ Item + Origin ];
   
    float2 Corner = float2( Vertex & 1, Vertex >> 1 );
    float2 Position = Instance.Bounds.xy + Corner * Instance.Bounds.zw;
    
    float2 Limited = clamp( Position, Instance.Clip.xy, Instance.Clip.zw );
    float2 Ratio = ( Limited - Instance.Bounds.xy ) / Instance.Bounds.zw;
    
    CFragment Fragment;
    
    Fragment.Position = float4( Limited * Projection.xy + Projection.zw, 0.0, 1.0 );
    Fragment.Color = lerp( Unpack( Instance.ColorStart ), Unpack( Instance.ColorFinish ), ( Instance.Flags & 4 ) ? Ratio.x : Ratio.y );
    Fragment.Local = Limited - Instance.Bounds.xy - Instance.Bounds.zw * 0.5;
    Fragment.Extent = Instance.Bounds.zw * 0.5 - Instance.Inflate;
    Fragment.Source = Instance.Source.xy + Ratio * Instance.Source.zw;
    Fragment.Shape = float3( Instance.Rounding, Instance.Thickness, Instance.Softness );

    Fragment.Flags = Instance.Flags;
    Fragment.Extra = Instance.Source;

    return Fragment;
}
)";

static const char* const HlslPixel = R"(
Texture2D Sheet : register( t0 );
SamplerState Smooth : register( s0 );

cbuffer CScreen : register( b0 )
{
    float4 Projection;
    uint Origin;
    float Moment;
};

struct CFragment
{
    float4 Position : SV_Position;
    float4 Color : COLOR0;

    float2 Local : TEXCOORD0;
    float2 Extent : TEXCOORD1;
    float2 Source : TEXCOORD2;

    nointerpolation float3 Shape : TEXCOORD3;
    nointerpolation uint Flags : TEXCOORD4;
    nointerpolation float4 Extra : TEXCOORD5;
};

#define Float2 float2
#define Float3 float3
#define Float4 float4

#define Lerp lerp
#define Saturate saturate

#define Fract frac
#define AtlasSample( Where ) Sheet.Sample( Smooth, Where )

float4 MainFragment( CFragment Fragment ) : SV_Target
{
    float4 Final = Fragment.Color;
    float2 Local = Fragment.Local;

    float2 Extent = Fragment.Extent;
    float2 Screen = Fragment.Position.xy;

    float Field = 0.0;

    if ( Fragment.Flags & 1 )
    {
        Final.a *= Sheet.Sample( Smooth, Fragment.Source ).r;
        Field = -1.0;
    }
    else
    {
        if ( Fragment.Flags & 8 )
        {
            Final *= Sheet.Sample( Smooth, Fragment.Source );
        }

        if ( Fragment.Flags & 32 )
        {
            float2 Start = ( Fragment.Extra.xy - 0.5 ) * Fragment.Extent * 2.0;
            float2 Finish = ( Fragment.Extra.zw - 0.5 ) * Fragment.Extent * 2.0;

            float2 Path = Finish - Start;
            float Along = saturate( dot( Fragment.Local - Start, Path ) / max( dot( Path, Path ), 0.0001 ) );

            Field = length( Fragment.Local - Start - Path * Along ) - Fragment.Shape.y * 0.5;
        }
        else if ( Fragment.Flags & 16 )
        {
            float2 Point = Fragment.Local;
            float2 Reach = Fragment.Extent;

            uint Turn = ( uint )( Fragment.Extra.x + 0.5 );
            if ( Turn == 1 )
            {
                Point = float2( Point.y, Point.x );
                Reach = Reach.yx;
            }

            if ( Turn == 2 )
            {
                Point.y = -Point.y;
            }

            if ( Turn == 3 )
            {
                Point = float2( Point.y, -Point.x );
                Reach = Reach.yx;
            }

            float Along = ( Point.y + Reach.y ) / max( Reach.y * 2.0, 0.001 );
            Field = abs( Point.x ) - Reach.x * ( 1.0 - Along );
        }
        else if ( Fragment.Flags & 64 )
        {
            float2 Point = Fragment.Local;
            float Span = length( Point );

            float Outer = min( Fragment.Extent.x, Fragment.Extent.y );

            float Inner = Fragment.Extra.z * Outer;
            float Turn = atan2( Point.y, Point.x ) - Fragment.Extra.x;

            Turn = Turn - floor( Turn / 6.2831853 ) * 6.2831853;

            float Band = max( Span - Outer, Inner - Span );
            float Half = Fragment.Extra.y * 0.5;
            float Slice = ( abs( Turn - Half ) - Half ) * max( Span, 1.0 );

            Field = Fragment.Extra.y >= 6.2831853 ? Band : max( Band, Slice );
        }
        else
        {
            float2 Reach = abs( Fragment.Local ) - Fragment.Extent + Fragment.Shape.x;
            Field = length( max( Reach, 0.0 ) ) + min( max( Reach.x, Reach.y ), 0.0 ) - Fragment.Shape.x;
            
            if ( Fragment.Flags & 2 )
            {
                Field = abs( Field + Fragment.Shape.y * 0.5 ) - Fragment.Shape.y * 0.5;
            }
        }

        float Edge = max( Fragment.Shape.z, 0.25 );
        Final.a *= 1.0 - smoothstep( -Edge, Edge, Field );
    }

    /*ATLAS_EFFECT*/
    return Final;
}
)";

static const char* const GlslVertex = R"(
struct CInstance
{
    vec4 Bounds;

    vec4 Clip;
    vec4 Source;

    uint ColorStart;
    uint ColorFinish;

    float Rounding;
    float Thickness;

    uint Flags;

    float Softness;
    float Inflate;

    uint Reserved;
};

layout( std430, binding = 0 ) readonly buffer CItems
{
    CInstance Items[];
};

uniform vec4 Projection;
uniform uint Origin;
uniform float Moment;

out vec4 Color;
out vec2 Local;

out vec2 Extent;
out vec2 Spot;

flat out vec3 Shape;
flat out uint Flags;
flat out vec4 Extra;

vec4 Unpack( uint Value )
{
    return vec4( Value & 255u, ( Value >> 8 ) & 255u, ( Value >> 16 ) & 255u, Value >> 24 ) / 255.0;
}

void main()
{
    CInstance Instance = Items[ uint( gl_InstanceID ) + Origin ];

    vec2 Corner = vec2( gl_VertexID & 1, gl_VertexID >> 1 );
    vec2 Position = Instance.Bounds.xy + Corner * Instance.Bounds.zw;

    vec2 Limited = clamp( Position, Instance.Clip.xy, Instance.Clip.zw );
    vec2 Ratio = ( Limited - Instance.Bounds.xy ) / Instance.Bounds.zw;

    gl_Position = vec4( Limited * Projection.xy + Projection.zw, 0.0, 1.0 );
   
    Color = mix( Unpack( Instance.ColorStart ), Unpack( Instance.ColorFinish ), ( Instance.Flags & 4u ) != 0u ? Ratio.x : Ratio.y );
    Local = Limited - Instance.Bounds.xy - Instance.Bounds.zw * 0.5;
    
    Extent = Instance.Bounds.zw * 0.5 - Instance.Inflate;
   
    Spot = Instance.Source.xy + Ratio * Instance.Source.zw;
    Shape = vec3( Instance.Rounding, Instance.Thickness, Instance.Softness );

    Flags = Instance.Flags;
    Extra = Instance.Source;
}
)";

static const char* const GlslPixel = R"(
uniform sampler2D Sheet;
uniform float Moment;

in vec4 Color;

in vec2 Local;
in vec2 Extent;
in vec2 Spot;

flat in vec3 Shape;
flat in uint Flags;
flat in vec4 Extra;

out vec4 Result;

#define Float2 vec2
#define Float3 vec3
#define Float4 vec4

#define Lerp mix
#define Saturate( Value ) clamp( Value, 0.0, 1.0 )

#define Fract fract
#define AtlasSample( Where ) texture( Sheet, Where )

void main()
{
    vec4 Final = Color;
    vec2 Screen = gl_FragCoord.xy;

    float Field = 0.0;
    if ( ( Flags & 1u ) != 0u )
    {
        Final.a *= texture( Sheet, Spot ).r;
        Field = -1.0;
    }
    else
    {
        if ( ( Flags & 8u ) != 0u )
        {
            Final *= texture( Sheet, Spot );
        }

        if ( ( Flags & 32u ) != 0u )
        {
            vec2 Start = ( Extra.xy - 0.5 ) * Extent * 2.0;
            vec2 Finish = ( Extra.zw - 0.5 ) * Extent * 2.0;

            vec2 Path = Finish - Start;
            float Along = clamp( dot( Local - Start, Path ) / max( dot( Path, Path ), 0.0001 ), 0.0, 1.0 );

            Field = length( Local - Start - Path * Along ) - Shape.y * 0.5;
        }
        else if ( ( Flags & 16u ) != 0u )
        {
            vec2 Point = Local;
            vec2 Limit = Extent;

            uint Turn = uint( Extra.x + 0.5 );
            if ( Turn == 1u )
            {
                Point = vec2( Point.y, Point.x );
                Limit = Limit.yx;
            }

            if ( Turn == 2u )
            {
                Point.y = -Point.y;
            }

            if ( Turn == 3u )
            {
                Point = vec2( Point.y, -Point.x );
                Limit = Limit.yx;
            }

            float Along = ( Point.y + Limit.y ) / max( Limit.y * 2.0, 0.001 );
            Field = abs( Point.x ) - Limit.x * ( 1.0 - Along );
        }
        else if ( ( Flags & 64u ) != 0u )
        {
            vec2 Point = Local;

            float Span = length( Point );
            float Outer = min( Extent.x, Extent.y );

            float Inner = Extra.z * Outer;
            float Turn = atan( Point.y, Point.x ) - Extra.x;

            Turn = Turn - floor( Turn / 6.2831853 ) * 6.2831853;

            float Band = max( Span - Outer, Inner - Span );
            float Half = Extra.y * 0.5;
            float Slice = ( abs( Turn - Half ) - Half ) * max( Span, 1.0 );

            Field = Extra.y >= 6.2831853 ? Band : max( Band, Slice );
        }
        else
        {
            vec2 Reach = abs( Local ) - Extent + Shape.x;
            Field = length( max( Reach, 0.0 ) ) + min( max( Reach.x, Reach.y ), 0.0 ) - Shape.x;
            
            if ( ( Flags & 2u ) != 0u )
            {
                Field = abs( Field + Shape.y * 0.5 ) - Shape.y * 0.5;
            }
        }

        float Edge = max( Shape.z, 0.25 );
        Final.a *= 1.0 - smoothstep( -Edge, Edge, Field );
    }

    /*ATLAS_EFFECT*/
    Result = Final;
}
)";

static const char* const VulkanVertex = R"(
struct CInstance
{
    vec4 Bounds;

    vec4 Clip;
    vec4 Source;

    uint ColorStart;
    uint ColorFinish;

    float Rounding;
    float Thickness;

    uint Flags;

    float Softness;
    float Inflate;

    uint Reserved;
};

layout( std430, set = 0, binding = 0 ) readonly buffer CItems
{
    CInstance Items[];
};

layout( push_constant ) uniform CScreen
{
    vec4 Projection;
    float Moment;
};

layout( location = 0 ) out vec4 Color;
layout( location = 1 ) out vec2 Local;
layout( location = 2 ) out vec2 Extent;
layout( location = 3 ) out vec2 Spot;
layout( location = 4 ) flat out vec3 Shape;
layout( location = 5 ) flat out uint Flags;
layout( location = 6 ) flat out vec4 Extra;

vec4 Unpack( uint Value )
{
    return vec4( Value & 255u, ( Value >> 8 ) & 255u, ( Value >> 16 ) & 255u, Value >> 24 ) / 255.0;
}

void main()
{
    CInstance Instance = Items[ gl_InstanceIndex ];
    
    vec2 Corner = vec2( gl_VertexIndex & 1, gl_VertexIndex >> 1 );
    vec2 Position = Instance.Bounds.xy + Corner * Instance.Bounds.zw;
    
    vec2 Limited = clamp( Position, Instance.Clip.xy, Instance.Clip.zw );
    vec2 Ratio = ( Limited - Instance.Bounds.xy ) / Instance.Bounds.zw;
    
    gl_Position = vec4( Limited * Projection.xy + Projection.zw, 0.0, 1.0 );
    Color = mix( Unpack( Instance.ColorStart ), Unpack( Instance.ColorFinish ), ( Instance.Flags & 4u ) != 0u ? Ratio.x : Ratio.y );
    
    Local = Limited - Instance.Bounds.xy - Instance.Bounds.zw * 0.5;
    Extent = Instance.Bounds.zw * 0.5 - Instance.Inflate;

    Spot = Instance.Source.xy + Ratio * Instance.Source.zw;
    Shape = vec3( Instance.Rounding, Instance.Thickness, Instance.Softness );

    Flags = Instance.Flags;
    Extra = Instance.Source;
}
)";

static const char* const VulkanPixel = R"(
layout( set = 1, binding = 0 ) uniform sampler2D Sheet;
layout( push_constant ) uniform CScreen
{
    vec4 Projection;
    float Moment;
};

layout( location = 0 ) in vec4 Color;
layout( location = 1 ) in vec2 Local;

layout( location = 2 ) in vec2 Extent;
layout( location = 3 ) in vec2 Spot;

layout( location = 4 ) flat in vec3 Shape;
layout( location = 5 ) flat in uint Flags;
layout( location = 6 ) flat in vec4 Extra;

layout( location = 0 ) out vec4 Result;

#define Float2 vec2
#define Float3 vec3
#define Float4 vec4

#define Lerp mix
#define Saturate( Value ) clamp( Value, 0.0, 1.0 )

#define Fract fract
#define AtlasSample( Where ) texture( Sheet, Where )

void main()
{
    vec4 Final = Color;

    vec2 Screen = gl_FragCoord.xy;
    float Field = 0.0;

    if ( ( Flags & 1u ) != 0u )
    {
        Final.a *= texture( Sheet, Spot ).r;
        Field = -1.0;
    }
    else
    {
        if ( ( Flags & 8u ) != 0u )
        {
            Final *= texture( Sheet, Spot );
        }

        if ( ( Flags & 32u ) != 0u )
        {
            vec2 Start = ( Extra.xy - 0.5 ) * Extent * 2.0;
            vec2 Finish = ( Extra.zw - 0.5 ) * Extent * 2.0;

            vec2 Path = Finish - Start;
            float Along = clamp( dot( Local - Start, Path ) / max( dot( Path, Path ), 0.0001 ), 0.0, 1.0 );

            Field = length( Local - Start - Path * Along ) - Shape.y * 0.5;
        }
        else if ( ( Flags & 16u ) != 0u )
        {
            vec2 Point = Local;
            vec2 Limit = Extent;

            uint Turn = uint( Extra.x + 0.5 );
            if ( Turn == 1u )
            {
                Point = vec2( Point.y, Point.x );
                Limit = Limit.yx;
            }

            if ( Turn == 2u )
            {
                Point.y = -Point.y;
            }

            if ( Turn == 3u )
            {
                Point = vec2( Point.y, -Point.x );
                Limit = Limit.yx;
            }

            float Along = ( Point.y + Limit.y ) / max( Limit.y * 2.0, 0.001 );
            Field = abs( Point.x ) - Limit.x * ( 1.0 - Along );
        }
        else if ( ( Flags & 64u ) != 0u )
        {
            vec2 Point = Local;

            float Span = length( Point );
            float Outer = min( Extent.x, Extent.y );

            float Inner = Extra.z * Outer;
            float Turn = atan( Point.y, Point.x ) - Extra.x;

            Turn = Turn - floor( Turn / 6.2831853 ) * 6.2831853;

            float Band = max( Span - Outer, Inner - Span );
            float Half = Extra.y * 0.5;

            float Slice = ( abs( Turn - Half ) - Half ) * max( Span, 1.0 );
            Field = Extra.y >= 6.2831853 ? Band : max( Band, Slice );
        }
        else
        {
            vec2 Reach = abs( Local ) - Extent + Shape.x;
            Field = length( max( Reach, 0.0 ) ) + min( max( Reach.x, Reach.y ), 0.0 ) - Shape.x;
            
            if ( ( Flags & 2u ) != 0u )
            {
                Field = abs( Field + Shape.y * 0.5 ) - Shape.y * 0.5;
            }
        }
        
        float Edge = max( Shape.z, 0.25 );
        Final.a *= 1.0 - smoothstep( -Edge, Edge, Field );
    }
    
    /*ATLAS_EFFECT*/
    Result = Final;
}
)";

static std::string Splice( const char* Source, const std::string& Body ) {
    std::string Result = Source;

    size_t Marker = Result.find( "/*ATLAS_EFFECT*/" );
    if ( Marker != std::string::npos )
        Result.replace( Marker, 16, Body );

    return Result;
}

unsigned int CShaders::Compose( const char* Name, const char* Body ) {
    if ( !Name || !Body )
        return 0;

    CEffect Effect;

    Effect.Name = Name;
    Effect.Body = Body;

    Effects.push_back( Effect );
    Version++;

    return ( unsigned int )Effects.size( );
}

int CShaders::Count( ) const {
    return ( int )Effects.size( );
}

const CEffect* CShaders::Fetch( unsigned int Effect ) const {
    if ( Effect == 0 || Effect > Effects.size( ) )
        return nullptr;

    return &Effects[ Effect - 1 ];
}

std::string CShaders::Vertex( int Language ) const {
    if ( Language == ShaderHlsl )
        return HlslVertex;

    if ( Language == ShaderGlsl )
        return std::string( "#version 430\n" ) + GlslVertex;

    return std::string( "#version 450\n" ) + VulkanVertex;
}

std::string CShaders::Pixel( unsigned int Effect, int Language ) const {
    const CEffect* Found = Fetch( Effect );
    std::string Body = Found ? Found->Body : std::string( );

    if ( Language == ShaderHlsl )
        return Splice( HlslPixel, Body );

    if ( Language == ShaderGlsl )
        return std::string( "#version 430\n" ) + Splice( GlslPixel, Body );

    return std::string( "#version 450\n" ) + Splice( VulkanPixel, Body );
}