#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <dwmapi.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <timeapi.h>
#include <functional>

#pragma comment( lib, "dwmapi.lib" )

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#include "Atlas.h"
#include "ElevenHost.h"
#include "OpenGLHost.h"
#include "TwelveHost.h"
#include "VulkanHost.h"

static bool Quit = false;
static bool Resized = false;

static int ClientWidth = 0;
static int ClientHeight = 0;

static std::function< void( ) >* Ticker = nullptr;

static LRESULT CALLBACK ProcessMessage( HWND Origin, UINT Message, WPARAM Primary, LPARAM Secondary ) {
    if ( Native->Translate( Origin, Message, Primary, Secondary ) )
        return Message == WM_SETCURSOR ? TRUE : 0;

    switch ( Message ) {
    case WM_CLOSE:
        Quit = true;

        return 0;
    case WM_SIZE: {
        int Across = ( int )( unsigned short )LOWORD( Secondary );
        int Down = ( int )( unsigned short )HIWORD( Secondary );

        if ( Across > 0 && Down > 0 ) {
            ClientWidth = Across;
            ClientHeight = Down;

            Resized = true;

            if ( Ticker )
                ( *Ticker )( );
        }

        return 0;
    }
    case WM_MOVE:
        if ( Ticker )
            ( *Ticker )( );

        return 0;
    case WM_DPICHANGED: {
        RECT* Suggested = ( RECT* )Secondary;

        SetWindowPos( Origin, nullptr, Suggested->left, Suggested->top, Suggested->right - Suggested->left, Suggested->bottom - Suggested->top, SWP_NOZORDER | SWP_NOACTIVATE );
        Atlas->Rescale( Native->Scale( ) );

        return 0;
    }
    case WM_ENTERSIZEMOVE:
        SetTimer( Origin, 1, 8, nullptr );
        return 0;

    case WM_EXITSIZEMOVE:
        KillTimer( Origin, 1 );
        return 0;

    case WM_TIMER:
        if ( Ticker )
            ( *Ticker )( );

        return 0;
    }

    return DefWindowProcW( Origin, Message, Primary, Secondary );
}

/* With Atlas shaders only have to be written once :) */

static const char* const SceneBody = R"Atlas(
Float4 ParamsOne = AtlasSample( Float2( 0.125, 0.5 ) );
Float4 ParamsTwo = AtlasSample( Float2( 0.375, 0.5 ) );
Float4 ParamsThree = AtlasSample( Float2( 0.625, 0.5 ) );

float Spin = ( ParamsOne.x * 255.0 + ParamsOne.y * 65280.0 ) / 65535.0 * 201.06193;
float GridFlag = ParamsOne.z;

float CameraYaw = ( ParamsTwo.x * 255.0 + ParamsTwo.y * 65280.0 ) / 65535.0 * 6.2831853;
float GlossFlag = ParamsTwo.z;

float CameraPitch = ( ParamsThree.x * 255.0 + ParamsThree.y * 65280.0 ) / 65535.0 * 1.27 + 0.08;
float Exposure = ParamsThree.z * 2.6 + 0.1;

float SpinSin = sin( Spin );
float SpinCos = cos( Spin );

float GemSpin = Spin * 0.90625;

float GemSin = sin( GemSpin );
float GemCos = cos( GemSpin );

Float3 GemAt = Float3( cos( Spin * 0.71875 ) * 2.6, 0.82 + sin( Spin * 1.3125 ) * 0.4, sin( Spin * 0.71875 ) * 2.6 );
Float3 BallAt = Float3( cos( Spin * -0.59375 + 2.4 ) * 2.1, 0.55, sin( Spin * -0.59375 + 2.4 ) * 2.1 );

#define AtlasTurn( Point ) Float3( ( Point ).x * SpinCos + ( Point ).z * SpinSin, ( Point ).y, ( Point ).z * SpinCos - ( Point ).x * SpinSin )
#define AtlasGemTurn( Point ) Float3( ( Point ).x * GemCos + ( Point ).z * GemSin, ( Point ).y, ( Point ).z * GemCos - ( Point ).x * GemSin )
#define AtlasBox( Point ) ( length( max( abs( AtlasTurn( ( Point ) - Float3( 0.0, 0.92, 0.0 ) ) ) - Float3( 0.62, 0.62, 0.62 ), 0.0 ) ) - 0.16 )
#define AtlasGem( Point ) ( ( dot( abs( AtlasGemTurn( ( Point ) - GemAt ) ), Float3( 1.0, 1.0, 1.0 ) ) - 0.92 ) * 0.5773 )
#define AtlasBall( Point ) ( length( ( Point ) - BallAt ) - 0.55 )
#define AtlasMap( Point ) min( min( ( Point ).y, AtlasBox( Point ) ), min( AtlasGem( Point ), AtlasBall( Point ) ) )

Float3 AimAt = Float3( 0.0, 0.8, 0.0 );
Float3 RayFrom = AimAt + Float3( sin( CameraYaw ) * cos( CameraPitch ), sin( CameraPitch ), cos( CameraYaw ) * cos( CameraPitch ) ) * 7.4;

Float3 Forward = normalize( AimAt - RayFrom );
Float3 Sideways = normalize( cross( Float3( 0.0, 1.0, 0.0 ), Forward ) );

Float3 Upward = cross( Forward, Sideways );

Float2 ViewUv = Local / Extent;
float Aspect = Extent.x / max( Extent.y, 1.0 );

Float3 RayPath = normalize( Forward * 1.9 + Sideways * ViewUv.x * Aspect - Upward * ViewUv.y );
Float3 LightPath = normalize( Float3( 0.52, 0.78, -0.36 ) );

float Travel = 0.0;
float Landed = -1.0;

for ( int Step = 0; Step < 72; Step++ )
{
    Float3 Probe = RayFrom + RayPath * Travel;
    float Range = AtlasMap( Probe );

    if ( Range < 0.0015 * Travel + 0.0008 )
    {
        Landed = 1.0;
        break;
    }

    Travel += Range;

    if ( Travel > 24.0 )
        break;
}

float SkyLine = Saturate( RayPath.y * 1.7 + 0.5 );
Float3 Shaded = Lerp( Float3( 0.045, 0.050, 0.085 ), Float3( 0.006, 0.007, 0.014 ), SkyLine );

float HorizonGlow = 1.0 - abs( RayPath.y );

HorizonGlow *= HorizonGlow;
HorizonGlow *= HorizonGlow;

Shaded += Float3( 0.045, 0.05, 0.10 ) * HorizonGlow;
Shaded += Float3( 0.09, 0.09, 0.11 ) * pow( Saturate( dot( RayPath, LightPath ) ), 24.0 );

if ( Landed > 0.0 && Travel < 24.0 )
{
    Float3 HitPoint = RayFrom + RayPath * Travel;

    Float2 Nudge = Float2( 0.0025, -0.0025 );
    Float3 NormalPath = normalize( Float3( Nudge.x, Nudge.y, Nudge.y ) * AtlasMap( HitPoint + Float3( Nudge.x, Nudge.y, Nudge.y ) ) + Float3( Nudge.y, Nudge.y, Nudge.x ) * AtlasMap( HitPoint + Float3( Nudge.y, Nudge.y, Nudge.x ) ) + Float3( Nudge.y, Nudge.x, Nudge.y ) * AtlasMap( HitPoint + Float3( Nudge.y, Nudge.x, Nudge.y ) ) + Float3( Nudge.x, Nudge.x, Nudge.x ) * AtlasMap( HitPoint + Float3( Nudge.x, Nudge.x, Nudge.x ) ) );

    float BoxRange = AtlasBox( HitPoint );
    float GemRange = AtlasGem( HitPoint );

    float BallRange = AtlasBall( HitPoint );
    float FloorRange = HitPoint.y;

    Float3 Paint = Float3( 0.42, 0.45, 0.98 );
    float Polish = 0.55;

    float OnFloor = -1.0;

    if ( FloorRange < BoxRange && FloorRange < GemRange && FloorRange < BallRange )
    {
        OnFloor = 1.0;
        Polish = 0.3;

        Paint = Float3( 0.052, 0.056, 0.082 );

        if ( GridFlag > 0.5 )
        {
            float AcrossLine = abs( Fract( HitPoint.x + 0.5 ) - 0.5 );
            float AlongLine = abs( Fract( HitPoint.z + 0.5 ) - 0.5 );

            float Stripe = 1.0 - smoothstep( 0.012, 0.05, min( AcrossLine, AlongLine ) );
            Paint = Lerp( Paint, Float3( 0.16, 0.18, 0.30 ), Stripe * 0.75 );
        }
    }
    else if ( GemRange < BoxRange && GemRange < BallRange )
    {
        Paint = Float3( 0.10, 0.75, 0.52 );
        Polish = 0.8;
    }
    else if ( BallRange < BoxRange )
    {
        Paint = Float3( 0.72, 0.76, 0.88 );
        Polish = 1.0;
    }

    float ShadowTone = 1.0;
    float ShadowTravel = 0.03;

    for ( int Step = 0; Step < 20; Step++ )
    {
        Float3 ShadowProbe = HitPoint + LightPath * ShadowTravel;
        float ShadowRange = AtlasMap( ShadowProbe );

        ShadowTone = min( ShadowTone, Saturate( 11.0 * ShadowRange / ShadowTravel ) );
        ShadowTravel += clamp( ShadowRange, 0.02, 0.4 );

        if ( ShadowTone < 0.02 || ShadowTravel > 9.0 )
            break;
    }

    float Occlusion = 0.0;
    float OcclusionSpan = 0.09;

    for ( int Step = 0; Step < 5; Step++ )
    {
        float OcclusionRange = AtlasMap( HitPoint + NormalPath * OcclusionSpan );

        Occlusion += ( OcclusionSpan - OcclusionRange ) / OcclusionSpan;
        OcclusionSpan += 0.11;
    }

    float Openness = Saturate( 1.0 - Occlusion * 0.17 );

    float Diffuse = Saturate( dot( NormalPath, LightPath ) ) * ShadowTone;
    float Ambient = ( 0.14 + 0.12 * Saturate( NormalPath.y ) ) * Openness;

    Float3 HalfPath = normalize( LightPath - RayPath );
    float Gleam = pow( Saturate( dot( NormalPath, HalfPath ) ), 64.0 ) * Polish * ShadowTone;

    float Fresnel = 1.0 - Saturate( dot( NormalPath, -RayPath ) );

    Fresnel *= Fresnel;
    Fresnel *= Fresnel;

    float FillGlow = Saturate( dot( NormalPath, normalize( Float3( -0.62, 0.30, 0.52 ) ) ) ) * Openness;
    float RimGlow = pow( 1.0 - Saturate( dot( NormalPath, -RayPath ) ), 3.0 ) * Saturate( NormalPath.y * 0.5 + 0.5 ) * Openness;

    Shaded = Paint * ( Ambient + Diffuse * 0.92 + FillGlow * 0.16 );

    Shaded += Float3( 1.0, 1.0, 1.0 ) * Gleam * 0.85;
    Shaded += Float3( 0.28, 0.33, 0.55 ) * RimGlow * 0.30;

    Float3 Bounce = reflect( RayPath, NormalPath );

    if ( GlossFlag > 0.5 && ( OnFloor > 0.0 || Polish > 0.7 ) )
    {
        Float3 MirrorFrom = HitPoint + NormalPath * 0.03;

        float MirrorTravel = 0.02;
        float MirrorLanded = -1.0;

        for ( int Step = 0; Step < 28; Step++ )
        {
            Float3 MirrorProbe = MirrorFrom + Bounce * MirrorTravel;
            float MirrorRange = min( AtlasBox( MirrorProbe ), min( AtlasGem( MirrorProbe ), AtlasBall( MirrorProbe ) ) );

            if ( MirrorRange < 0.004 * MirrorTravel + 0.001 )
            {
                MirrorLanded = 1.0;
                break;
            }

            MirrorTravel += MirrorRange;

            if ( MirrorTravel > 10.0 )
                break;
        }

        float MirrorGrip = OnFloor > 0.0 ? 0.16 + 0.5 * Fresnel : Polish * ( 0.22 + 0.55 * Fresnel );

        if ( MirrorLanded > 0.0 )
        {
            Float3 MirrorPoint = MirrorFrom + Bounce * MirrorTravel;

            float MirrorBox = AtlasBox( MirrorPoint );
            float MirrorGem = AtlasGem( MirrorPoint );

            Float3 MirrorPaint = Float3( 0.42, 0.45, 0.98 );

            if ( MirrorGem < MirrorBox )
                MirrorPaint = Float3( 0.10, 0.75, 0.52 );

            if ( AtlasBall( MirrorPoint ) < min( MirrorBox, MirrorGem ) )
                MirrorPaint = Float3( 0.72, 0.76, 0.88 );

            float MirrorTone = 0.35 + 0.55 * Saturate( dot( LightPath, Bounce ) * 0.5 + 0.5 );
            float MirrorFade = Saturate( 1.0 - MirrorTravel * 0.16 );

            Shaded = Lerp( Shaded, MirrorPaint * MirrorTone, MirrorGrip * MirrorFade );
        }
        else
        {
            Float3 MirrorSky = Lerp( Float3( 0.03, 0.035, 0.06 ), Float3( 0.20, 0.24, 0.40 ), Saturate( Bounce.y * 0.8 + 0.4 ) );
            Shaded = Lerp( Shaded, MirrorSky, MirrorGrip * 0.6 );
        }
    }
    else
    {
        Float3 EnvGlow = Lerp( Float3( 0.03, 0.035, 0.06 ), Float3( 0.20, 0.24, 0.40 ), Saturate( Bounce.y * 0.8 + 0.4 ) );
        Shaded += EnvGlow * Polish * ( 0.25 + 0.75 * Fresnel );
    }

    float Mist = 1.0 - exp( -0.0022 * Travel * Travel );
    Shaded = Lerp( Shaded, Float3( 0.010, 0.011, 0.020 ), Mist );
}

Shaded = Shaded * Exposure;
Float3 Graded = ( Shaded * ( Shaded * 2.51 + Float3( 0.03, 0.03, 0.03 ) ) ) / ( Shaded * ( Shaded * 2.43 + Float3( 0.59, 0.59, 0.59 ) ) + Float3( 0.14, 0.14, 0.14 ) );

float Vignette = 1.0 - dot( ViewUv, ViewUv ) * 0.42;
Final.rgb = pow( Saturate( Graded * Vignette ), Float3( 0.93, 0.93, 0.93 ) );
)Atlas";

static const char* const BlurBody = R"Atlas(
Float2 Coord = Local / Extent * 0.5 + Float2( 0.5, 0.5 );
Float2 Texel = Float2( 0.5 / max( Extent.x, 1.0 ), 0.5 / max( Extent.y, 1.0 ) );

float Radius = 15.0;
float Golden = 2.3999632;

Float3 Sum = Float3( 0.0, 0.0, 0.0 );
float Total = 0.0;

for ( int Tap = 0; Tap < 48; Tap++ )
{
    float Index = float( Tap ) + 0.5;
    float Ring = sqrt( Index / 48.0 );

    float Angle = Index * Golden;
    float Weight = exp( -Ring * Ring * 1.5 );

    Float2 Offset = Float2( cos( Angle ), sin( Angle ) ) * Ring * Radius;
    Sum = Sum + AtlasSample( Coord + Offset * Texel ).rgb * Weight;

    Total = Total + Weight;
}

Final.rgb = Sum / Total;
)Atlas";

static const char* const AcrylicBody = R"Atlas(
Float2 Coord = Local / Extent * 0.5 + Float2( 0.5, 0.5 );
Float2 Texel = Float2( 0.5 / max( Extent.x, 1.0 ), 0.5 / max( Extent.y, 1.0 ) );

float Radius = 26.0;
float Golden = 2.3999632;

Float3 Sum = Float3( 0.0, 0.0, 0.0 );
float Total = 0.0;

for ( int Tap = 0; Tap < 64; Tap++ )
{
    float Index = float( Tap ) + 0.5;
    float Ring = sqrt( Index / 64.0 );

    float Angle = Index * Golden;
    float Weight = exp( -Ring * Ring * 1.1 );

    Float2 Offset = Float2( cos( Angle ), sin( Angle ) ) * Ring * Radius;
    Sum = Sum + AtlasSample( Coord + Offset * Texel ).rgb * Weight;

    Total = Total + Weight;
}

Float3 Frost = Sum / Total;
float Gray = dot( Frost, Float3( 0.3333, 0.3333, 0.3333 ) );

Frost = Lerp( Frost, Float3( Gray, Gray, Gray ), 0.15 );
Final.rgb = Frost * 0.9 + Float3( 0.05, 0.06, 0.085 );
)Atlas";

static const char* const ChromaticBody = R"Atlas(
Float2 Coord = Local / Extent * 0.5 + Float2( 0.5, 0.5 );
Float2 Aim = Coord - Float2( 0.5, 0.5 );

float Shift = 7.0 / max( Extent.x, 1.0 );

Final.r = AtlasSample( Coord + Aim * Shift ).r;
Final.g = AtlasSample( Coord ).g;
Final.b = AtlasSample( Coord - Aim * Shift ).b;
)Atlas";

static const char* const PixelBody = R"Atlas(
Float2 Coord = Local / Extent * 0.5 + Float2( 0.5, 0.5 );
float Cells = 46.0;

Coord = ( Float2( floor( Coord.x * Cells ), floor( Coord.y * Cells ) ) + Float2( 0.5, 0.5 ) ) / Cells;
Final.rgb = AtlasSample( Coord ).rgb;
)Atlas";

static const char* const RippleBody = R"Atlas(
Float2 Coord = Local / Extent * 0.5 + Float2( 0.5, 0.5 );

float WaveX = sin( Coord.y * 28.0 + Moment * 2.6 ) * 0.010;
float WaveY = cos( Coord.x * 24.0 + Moment * 2.0 ) * 0.010;

Final.rgb = AtlasSample( Coord + Float2( WaveX, WaveY ) ).rgb;
)Atlas";

static const char* const BloomBody = R"Atlas(
Float2 Coord = Local / Extent * 0.5 + Float2( 0.5, 0.5 );
float Step = 5.0 / max( Extent.x, 1.0 );

Float3 Base = AtlasSample( Coord ).rgb;
Float3 Halo = ( AtlasSample( Coord + Float2( Step, 0.0 ) ).rgb + AtlasSample( Coord - Float2( Step, 0.0 ) ).rgb + AtlasSample( Coord + Float2( 0.0, Step ) ).rgb + AtlasSample( Coord - Float2( 0.0, Step ) ).rgb + AtlasSample( Coord + Float2( Step, Step ) ).rgb + AtlasSample( Coord - Float2( Step, Step ) ).rgb ) * 0.1667;

Float3 Bright = max( Halo - Float3( 0.55, 0.55, 0.55 ), Float3( 0.0, 0.0, 0.0 ) );
Final.rgb = Base + Bright * 1.9;
)Atlas";

static const char* const MonoBody = R"Atlas(
float Mixed = dot( Final.rgb, Float3( 0.299, 0.587, 0.114 ) );
Final.rgb = Float3( Mixed, Mixed, Mixed );
)Atlas";

static void ApplyTitleBar( HWND Window, bool DarkMode ) {
    BOOL Enabled = DarkMode ? TRUE : FALSE;

    if ( FAILED( DwmSetWindowAttribute( Window, DWMWA_USE_IMMERSIVE_DARK_MODE, &Enabled, sizeof( Enabled ) ) ) )
        DwmSetWindowAttribute( Window, 19, &Enabled, sizeof( Enabled ) );
}

int WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR CommandLine, int ) {
    SetProcessDPIAware( );
    timeBeginPeriod( 1 );

    WNDCLASSEXW Description = { };

    Description.cbSize = sizeof( WNDCLASSEXW );
    Description.style = CS_DBLCLKS;

    Description.lpfnWndProc = ProcessMessage;
    Description.hInstance = GetModuleHandleW( nullptr );

    Description.hCursor = LoadCursorW( nullptr, ( const wchar_t* )IDC_ARROW );
    Description.lpszClassName = L"AtlasSample";

    if ( !RegisterClassExW( &Description ) )
        return 0;

    RECT Frame = { 0, 0, 1480, 860 };
    AdjustWindowRect( &Frame, WS_OVERLAPPEDWINDOW, FALSE );

    int FullWidth = ( int )( Frame.right - Frame.left );
    int FullHeight = ( int )( Frame.bottom - Frame.top );

    int AnchorLeft = ( GetSystemMetrics( SM_CXSCREEN ) - FullWidth ) / 2;
    int AnchorTop = ( GetSystemMetrics( SM_CYSCREEN ) - FullHeight ) / 2;

    HWND Handle = CreateWindowExW( 0, L"AtlasSample", L"Atlas", WS_OVERLAPPEDWINDOW, AnchorLeft, AnchorTop, FullWidth, FullHeight, nullptr, nullptr, Description.hInstance, nullptr );
    if ( !Handle )
        return 0;

    RECT Client = { };
    GetClientRect( Handle, &Client );

    ClientWidth = ( int )Client.right;
    ClientHeight = ( int )Client.bottom;

    Native->Create( Handle );

    if ( !Atlas->Create( ) )
        return 0;

    Atlas->Rescale( Native->Scale( ) );
    bool Restored = Atlas->Load( "atlas.layout" );

    ApplyTitleBar( Handle, true );
    ShowWindow( Handle, SW_SHOW );

    std::vector< CHost* > Roster;
    std::vector< const char* > Names;

    Roster.push_back( ElevenHost.get( ) );
    Names.push_back( "DirectX 11" );

    Roster.push_back( TwelveHost.get( ) );
    Names.push_back( "DirectX 12" );

    Roster.push_back( OpenGLHost.get( ) );
    Names.push_back( "OpenGL" );

#if ATLAS_VULKAN
    Roster.push_back( VulkanHost.get( ) );
    Names.push_back( "Vulkan" );
#endif

    int Selected = CommandLine ? atoi( CommandLine ) : 0;
    if ( Selected < 0 || Selected >= ( int )Roster.size( ) )
        Selected = 0;

    int Running = Selected;

    if ( !Roster[ Running ]->Create( Handle, ClientWidth, ClientHeight ) ) {
        Running = 0;
        Selected = 0;

        if ( !Roster[ Running ]->Create( Handle, ClientWidth, ClientHeight ) )
            return 0;
    }

    unsigned char Params[ 16 ] = { };
    unsigned long long Dials = Roster[ Running ]->Graphics( )->CreateImage( Params, 4, 1 );

    std::vector< unsigned char > Photo;

    int PhotoWidth = 0;
    int PhotoHeight = 0;

    bool PhotoReady = Pictures->Load( "C:\\Windows\\Web\\Wallpaper\\Windows\\img0.jpg", Photo, PhotoWidth, PhotoHeight, 1280 );
    unsigned long long Album = 0;

    if ( PhotoReady )
        Album = Roster[ Running ]->Graphics( )->CreateImage( Photo.data( ), PhotoWidth, PhotoHeight );

    const char* FolderSvg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\"><path fill=\"#5684FF\" d=\"M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z\"/></svg>";
    std::vector< unsigned char > IconPixels;

    int IconWidth = 0;
    int IconHeight = 0;

    bool IconReady = Pictures->VectorBytes( ( const unsigned char* )FolderSvg, strlen( FolderSvg ), IconPixels, IconWidth, IconHeight, 32 );
    unsigned long long Folder = 0;

    if ( IconReady )
        Folder = Roster[ Running ]->Graphics( )->CreateImage( IconPixels.data( ), IconWidth, IconHeight );

    unsigned int Scene = Shaders->Compose( "Scene", SceneBody );

    unsigned int Blur = Shaders->Compose( "Blur", BlurBody );
    unsigned int Acrylic = Shaders->Compose( "Acrylic", AcrylicBody );

    unsigned int Chromatic = Shaders->Compose( "Chromatic", ChromaticBody );
    unsigned int Pixel = Shaders->Compose( "Pixel", PixelBody );

    unsigned int Ripple = Shaders->Compose( "Ripple", RippleBody );
    unsigned int Bloom = Shaders->Compose( "Bloom", BloomBody );

    unsigned int Mono = Shaders->Compose( "Mono", MonoBody );
    unsigned int Applied[ 8 ] = { 0, Blur, Acrylic, Chromatic, Pixel, Ripple, Bloom, Mono };

    const char* Themes[ 2 ] = { "Dark", "Light" };
    const char* Looks[ 8 ] = { "None", "Gaussian Blur", "Acrylic Glass", "Chromatic", "Pixelate", "Ripple", "Bloom", "Grayscale" };

    const char* Paints[ 4 ] = { "Indigo", "Teal", "Rose", "Amber" };
    const char* Flavors[ 4 ] = { "Vanilla", "Chocolate", "Mint", "Mango" };

    const char* Places[ 3 ] = { "Left", "Center", "Right" };
    const CColor Tints[ 4 ] = { CColor( 99, 102, 241 ), CColor( 45, 212, 191 ), CColor( 244, 114, 182 ), CColor( 245, 158, 11 ) };

    int Theme = 0;
    int Look = 0;

    int Paint = 0;
    int Flavor = 0;

    int Alignment = 1;
    int Portion = 1;

    int Presses = 0;
    bool VerticalSync = true;

    bool Grid = true;
    bool Gloss = true;

    bool FirstOption = true;

    float Exposure = 1.0f;
    float Volume = 64.0f;

    float Speed = 1.0f;
    double Winding = 0.6;

    float OrbitYaw = 0.7f;
    float OrbitPitch = 0.0f;

    float BaseYaw = 0.0f;
    float BasePitch = 0.0f;

    CVector Grab;

    char PersonName[ 64 ] = "Ada Lovelace";
    char Notebook[ 512 ] = "Multiline editing with selection,\nclipboard and undo. Try Ctrl+Z.";

    int Quantity = 12;
    float Threshold = 0.35f;

    int Rating = 3;
    float Tuning[ 3 ] = { 1.0f, 0.5f, 0.25f };

    bool Wireframe = false;
    int Fruit = 0;

    CColor Brush = CColor( 99, 102, 241 );
    bool DiagnosticsOpen = true;

    float Wave[ 96 ] = { };
    float Bars[ 12 ] = { };

    float Slices[ 5 ] = { 32.0f, 24.0f, 18.0f, 14.0f, 10.0f };

    bool Tasks[ 5 ] = { true, false, true, false, false };
    char TaskBuffer[ 64 ] = "";

    int SceneSel = 0;

    const char* Fruits[ 5 ] = { "Apple", "Banana", "Cherry", "Date", "Elderberry" };
    const char* Rows[ 3 ] = { "Alpha", "Beta", "Gamma" };

    std::function< void( ) > Tick = [ & ]( ) {
        if ( Resized ) {
            Resized = false;
            Roster[ Running ]->Resize( ClientWidth, ClientHeight );
        }

        Winding += ( double )Context->DeltaTime * Speed;

        double Turns = fmod( Winding, 201.06193 );
        int SpinCode = ( int )( Turns / 201.06193 * 65535.0 );

        float YawWrap = fmodf( OrbitYaw, 6.2831853f );
        if ( YawWrap < 0.0f )
            YawWrap += 6.2831853f;

        int YawCode = ( int )( YawWrap / 6.2831853f * 65535.0f );

        float PitchWorld = 0.42f + OrbitPitch;
        if ( PitchWorld < 0.08f )
            PitchWorld = 0.08f;

        if ( PitchWorld > 1.35f )
            PitchWorld = 1.35f;

        int PitchCode = ( int )( ( PitchWorld - 0.08f ) / 1.27f * 65535.0f );
        int ExposureCode = ( int )( ( Exposure - 0.1f ) / 2.6f * 255.0f );

        if ( ExposureCode < 0 )
            ExposureCode = 0;

        if ( ExposureCode > 255 )
            ExposureCode = 255;

        Params[ 0 ] = ( unsigned char )( SpinCode & 255 );
        Params[ 1 ] = ( unsigned char )( SpinCode >> 8 );

        Params[ 2 ] = Grid ? 255 : 0;
        Params[ 3 ] = 255;

        Params[ 4 ] = ( unsigned char )( YawCode & 255 );
        Params[ 5 ] = ( unsigned char )( YawCode >> 8 );

        Params[ 6 ] = Gloss ? 255 : 0;
        Params[ 7 ] = 255;

        Params[ 8 ] = ( unsigned char )( PitchCode & 255 );
        Params[ 9 ] = ( unsigned char )( PitchCode >> 8 );

        Params[ 10 ] = ( unsigned char )ExposureCode;
        Params[ 11 ] = 255;

        Params[ 12 ] = 0;
        Params[ 13 ] = 0;

        Params[ 14 ] = 0;
        Params[ 15 ] = 255;

        Roster[ Running ]->Graphics( )->UpdateImage( Dials, Params );
        Atlas->Begin( CVector( ( float )ClientWidth, ( float )ClientHeight ) );

        if ( !Restored && Context->FrameIndex == 2 ) {
            Docking->Adopt( "Scene", 0 );
            Docking->Adopt( "Tasks", 2 );
        }

        Docking->Space( CRectangle( 0.0f, 0.0f, ( float )ClientWidth, ( float )ClientHeight ) );

        if ( Frames->Begin( "Scene", nullptr, FrameDefault, CVector( 480.0f, 90.0f ), CVector( 520.0f, 460.0f ) ) ) {
            unsigned int Former = Canvas->Effect( Scene );
            CRectangle View = Widgets->Picture( Dials, CVector( Layout->Width( ), Layout->Width( ) * 0.5f ) );

            Canvas->Effect( Former );

            unsigned int OrbitZone = Context->Hash( "Scene Orbit" );
            bool OverView = View.Contains( Input->MousePosition ) && Frames->Beneath == Frames->Current && !Context->Covered( Input->MousePosition );

            if ( OverView && Input->MousePressed( 0 ) && Context->ActiveItem == 0 ) {
                Context->ActiveItem = OrbitZone;
                Grab = Input->MousePosition;

                BaseYaw = OrbitYaw;
                BasePitch = OrbitPitch;
            }

            if ( Context->ActiveItem == OrbitZone ) {
                if ( Input->MouseDown( 0 ) ) {
                    OrbitYaw = BaseYaw - ( Input->MousePosition.Horizontal - Grab.Horizontal ) * 0.011f;
                    OrbitPitch = BasePitch + ( Input->MousePosition.Vertical - Grab.Vertical ) * 0.009f;

                    if ( OrbitPitch > 0.9f )
                        OrbitPitch = 0.9f;

                    if ( OrbitPitch < -0.32f )
                        OrbitPitch = -0.32f;
                } else {
                    Context->ActiveItem = 0;
                }
            }

            Widgets->Faint( "Raymarched live on the graphics card, drag to orbit" );
            Widgets->Tooltip( "The whole scene is one Atlas shader effect running on every port" );

            Widgets->Slider( "Spin Speed", Speed, 0.0f, 3.0f );

            Widgets->Check( "Ground Grid", Grid );
            Widgets->Check( "Reflections", Gloss );

            Widgets->Slider( "Exposure", Exposure, 0.2f, 2.4f );
        }
        Frames->End( );

        for ( int Sample = 0; Sample < 96; Sample++ )
            Wave[ Sample ] = sinf( ( float )Context->Elapsed * 2.0f + ( float )Sample * 0.2f ) * ( 0.5f + 0.4f * sinf( ( float )Sample * 0.05f ) );

        for ( int Bucket = 0; Bucket < 12; Bucket++ )
            Bars[ Bucket ] = 0.5f + 0.5f * sinf( ( float )Context->Elapsed + ( float )Bucket * 0.5f );

        if ( Frames->Begin( "Tasks", nullptr, FrameDefault, CVector( 60.0f, 96.0f ), CVector( 600.0f, 470.0f ) ) ) {
            Widgets->Heading( "Today" );

            int Done = 0;
            for ( int Item = 0; Item < 5; Item++ ) {
                if ( Tasks[ Item ] )
                    Done++;
            }

            Widgets->Faint( Format->Print( "%d of 5 completed", Done ) );
            Widgets->Progress( ( float )Done / 5.0f );

            Widgets->Separator( );

            const char* TaskName[ 5 ] = { "Design review", "Fix the scroll bug", "Ship the v2 release", "Team standup at 3 pm", "Write the changelog" };

            for ( int Item = 0; Item < 5; Item++ ) {
                Context->PushIdentifier( Item );
                Widgets->Check( TaskName[ Item ], Tasks[ Item ] );

                Context->PopIdentifier( );
            }

            Widgets->Separator( );
            Widgets->Field( "", TaskBuffer, 64, "Add a task" );
        }
        Frames->End( );

        if ( Frames->Begin( "Controls", nullptr, FrameDefault, CVector( 14.0f, 76.0f ), CVector( 376.0f, 568.0f ) ) ) {
            if ( Widgets->BeginMenuBar( ) ) {
                if ( Widgets->BeginMenu( "File" ) ) {
                    if ( Widgets->MenuItem( "Save Layout", "Ctrl+S" ) )
                        Atlas->Save( "atlas.layout" );

                    if ( Widgets->MenuItem( "Load Layout" ) )
                        Atlas->Load( "atlas.layout" );

                    if ( Widgets->MenuItem( "Quit" ) )
                        Quit = true;

                    Widgets->EndMenu( );
                }

                if ( Widgets->BeginMenu( "View" ) ) {
                    if ( Widgets->MenuItem( "Dark Theme", nullptr, Theme == 0 ) ) {
                        Theme = 0;
                        Style->Dark( );
                    }

                    if ( Widgets->MenuItem( "Light Theme", nullptr, Theme == 1 ) ) {
                        Theme = 1;
                        Style->Light( );
                    }

                    if ( Widgets->MenuItem( "Diagnostics", nullptr, DiagnosticsOpen ) )
                        DiagnosticsOpen = !DiagnosticsOpen;

                    if ( Widgets->MenuItem( "Glass", nullptr, Style->Glass ) )
                        Style->Glass = !Style->Glass;

                    if ( Widgets->MenuItem( "Docking", nullptr, Docking->Enabled ) )
                        Docking->Enabled = !Docking->Enabled;

                    Widgets->EndMenu( );
                }

                Widgets->EndMenuBar( );
            }

            if ( Widgets->BeginTabs( "ControlTabs" ) ) {
                if ( Widgets->Tab( "Widgets" ) ) {
                    Widgets->Section( "Buttons" );

                    float Half = ( Layout->Width( ) - Style->Spacing ) * 0.5f;

                    if ( Widgets->Button( "Press Me", Half ) )
                        Presses++;

                    Widgets->Tooltip( "Widgets report interaction through their return value" );

                    Layout->SameLine( );

                    if ( Widgets->Button( "Reset", Half ) )
                        Presses = 0;

                    Widgets->Label( Format->Print( "Pressed %d times", Presses ) );

                    Widgets->Section( "Selection" );

                    Widgets->Check( "First Option", FirstOption );
                    Widgets->Toggle( "Wireframe", Wireframe );

                    Widgets->Radio( "Small", Portion, 0 );
                    Widgets->Radio( "Medium", Portion, 1 );

                    Widgets->Radio( "Large", Portion, 2 );

                    Widgets->Choice( "Flavor", Flavor, Flavors, 4 );
                    Widgets->Segments( "Alignment", Alignment, Places, 3 );

                    Widgets->Section( "Text" );

                    Widgets->Field( "Name", PersonName, 64, "your name" );
                    Widgets->Area( "Notes", Notebook, 512, 76.0f * Style->Scale );

                    Widgets->Section( "Values" );

                    Widgets->Slider( "Volume", Volume, 0.0f, 100.0f );
                    Widgets->SliderWhole( "Rating", Rating, 0, 5 );

                    Widgets->Number( "Quantity", Quantity );
                    Widgets->Decimal( "Threshold", Threshold, 0.01f );

                    Widgets->Drag( "Fine Tune", Tuning[ 0 ], 0.01f, -10.0f, 10.0f );
                    Widgets->Vector( "Position", Tuning, 3, -10.0f, 10.0f );

                    Widgets->Color( "Brush", Brush );
                    Widgets->Progress( 0.5f + 0.5f * sinf( ( float )Context->Elapsed * 1.4f ) );
                }

                if ( Widgets->Tab( "Appearance" ) ) {
                    Widgets->Section( "Graphics" );

                    Widgets->Choice( "Graphics Port", Selected, Names.data( ), ( int )Names.size( ) );
                    Widgets->Tooltip( "Switches the rendering backend while the application keeps running" );

                    Widgets->Check( "Vertical Sync", VerticalSync );

                    Widgets->Section( "Theme" );
                    Widgets->Segments( "Mode", Theme, Themes, 2 );

                    if ( Theme == 0 && Style->Backdrop.Red > 100 )
                        Style->Dark( );

                    if ( Theme == 1 && Style->Backdrop.Red < 100 )
                        Style->Light( );

                    if ( Widgets->Segments( "Accent", Paint, Paints, 4 ) ) {
                        Style->Accent = Tints[ Paint ];
                        Style->AccentSoft = Tints[ Paint ].Fade( 0.25f );
                    }

                    Widgets->Slider( "Corner Rounding", Style->Rounding, 0.0f, 24.0f );
                    Widgets->Slider( "Shadow Softness", Style->Softness, 1.0f, 44.0f );

                    Widgets->Slider( "Title Alignment", Style->TitleAlign, 0.0f, 1.0f );

                    Widgets->Check( "Shadows", Style->Shadows );
                    Widgets->Check( "Borders", Style->Borders );

                    Widgets->Check( "Smooth Scrolling", Style->ScrollSmooth );
                    Widgets->Check( "Adaptive Widgets", Style->Adaptive );
                }

                if ( Widgets->Tab( "Gallery" ) ) {
                    if ( Album ) {
                        float Wide = Layout->Width( );
                        float Tall = Wide * ( float )PhotoHeight / ( float )PhotoWidth;

                        unsigned int Former = Canvas->Effect( Applied[ Look ] );
                        Widgets->Picture( Album, CVector( Wide, Tall ) );

                        Canvas->Effect( Former );

                        Widgets->Wrapped( "Live GPU effect compiled once for every graphics port" );
                        Widgets->Choice( "Effect", Look, Looks, 8 );

                        Widgets->Tooltip( "One shader, portable across DirectX, OpenGL and Vulkan" );
                    } else {
                        Widgets->Faint( "No sample image was found on this machine" );
                    }
                }

                Widgets->EndTabs( );
            }
        }
        Frames->End( );

        if ( Frames->Begin( "Charts", nullptr, FrameDefault, CVector( 406.0f, 76.0f ), CVector( 376.0f, 568.0f ) ) ) {
            if ( Widgets->BeginTabs( "ChartTabs" ) ) {
                if ( Widgets->Tab( "Graphs" ) ) {
                    Widgets->Pie( "Share", Slices, 5 );
                    Widgets->Pie( "Donut", Slices, 5, true );

                    Widgets->Plot( "Signal", Wave, 96 );
                    Widgets->Area( "Filled", Wave, 96 );

                    Widgets->Histogram( "Buckets", Bars, 12 );
                }

                if ( Widgets->Tab( "Data" ) ) {
                    if ( Widgets->Tree( "Scene Graph", Folder, true ) ) {
                        if ( Widgets->TreeLeaf( "Camera", SceneSel == 0 ) )
                            SceneSel = 0;

                        if ( Widgets->TreeLeaf( "Sun Light", SceneSel == 1 ) )
                            SceneSel = 1;

                        if ( Widgets->Tree( "Meshes", Folder, true ) ) {
                            if ( Widgets->TreeLeaf( "Cube", SceneSel == 2 ) )
                                SceneSel = 2;

                            if ( Widgets->TreeLeaf( "Sphere", SceneSel == 3 ) )
                                SceneSel = 3;

                            if ( Widgets->TreeLeaf( "Torus", SceneSel == 4 ) )
                                SceneSel = 4;

                            Widgets->TreePop( );
                        }

                        Widgets->TreePop( );
                    }

                    if ( Widgets->BeginCollapse( "Fruit List" ) ) {
                        Widgets->List( "Fruits", Fruit, Fruits, 5, 4 );
                        Widgets->EndCollapse( );
                    }

                    if ( Layout->BeginTable( "Grid", 3 ) ) {
                        Layout->TableSetup( "Name" );
                        Layout->TableSetup( "Value", 90.0f * Style->Scale );

                        Layout->TableSetup( "State" );
                        Layout->TableHeaders( );

                        for ( int Line = 0; Line < 3; Line++ ) {
                            Layout->TableRow( );

                            Layout->TableColumn( );
                            Widgets->Label( Rows[ Line ] );

                            Layout->TableColumn( );
                            Widgets->Label( Format->Print( "%d", Line * 17 ) );

                            Layout->TableColumn( );
                            Widgets->Faint( Line % 2 ? "on" : "off" );
                        }

                        Layout->EndTable( );
                    }
                }

                Widgets->EndTabs( );
            }
        }
        Frames->End( );

        if ( Frames->Begin( "About", nullptr, FrameDefault, CVector( 798.0f, 448.0f ), CVector( 372.0f, 236.0f ) ) ) {
            Widgets->Label( "An interface library for C++" );

            Widgets->Faint( "Written by a high school senior" );
            Widgets->Faint( "Fast and easy to use" );

            Widgets->Faint( "DirectX 11/12, OpenGL, Vulkan" );
        }
        Frames->End( );

        if ( DiagnosticsOpen )
            Atlas->Diagnostics( &DiagnosticsOpen );

        Atlas->End( );

        static int TitleTheme = -1;

        if ( Theme != TitleTheme ) {
            TitleTheme = Theme;
            ApplyTitleBar( Handle, Theme == 0 );
        }

        Roster[ Running ]->Begin( Style->Backdrop );
        Roster[ Running ]->Graphics( )->Render( Atlas->Data( ), Roster[ Running ]->Stream( ) );

        Roster[ Running ]->End( VerticalSync );

        if ( Selected != Running ) {
            Roster[ Running ]->Destroy( );

            if ( Roster[ Selected ]->Create( Handle, ClientWidth, ClientHeight ) ) {
                Running = Selected;
            } else {
                Roster[ Selected ]->Destroy( );
                Selected = Running;

                if ( !Roster[ Running ]->Create( Handle, ClientWidth, ClientHeight ) ) {
                    Quit = true;
                    return;
                }
            }

            Dials = Roster[ Running ]->Graphics( )->CreateImage( Params, 4, 1 );

            if ( PhotoReady )
                Album = Roster[ Running ]->Graphics( )->CreateImage( Photo.data( ), PhotoWidth, PhotoHeight );

            if ( IconReady )
                Folder = Roster[ Running ]->Graphics( )->CreateImage( IconPixels.data( ), IconWidth, IconHeight );
        }
    };

    Ticker = &Tick;

    while ( !Quit ) {
        MSG Message;

        while ( PeekMessageW( &Message, nullptr, 0, 0, PM_REMOVE ) ) {
            TranslateMessage( &Message );
            DispatchMessageW( &Message );
        }

        if ( Quit )
            break;

        Tick( );
    }

    Ticker = nullptr;
    timeEndPeriod( 1 );

    Atlas->Save( "atlas.layout" );
    Roster[ Running ]->Destroy( );

    Atlas->Destroy( );
    Native->Destroy( );

    DestroyWindow( Handle );
    UnregisterClassW( L"AtlasSample", Description.hInstance );

    return 0;
}