cmake_minimum_required( VERSION 3.20 )

project( Atlas LANGUAGES CXX )

set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

# Apple needs Objective-C++ for AppKit/UIKit and Metal. Enabling the language here,
# at top level, keeps the Windows build untouched.
if( APPLE )
    enable_language( OBJCXX )

    set( CMAKE_OBJCXX_STANDARD 20 )
    set( CMAKE_OBJCXX_STANDARD_REQUIRED ON )
    set( CMAKE_OBJCXX_EXTENSIONS OFF )
endif( )

# Output stays inside the CMake build tree. Writing into the repo-root Build folder would
# clobber the MSBuild artifacts that Atlas.vcxproj publishes into Package/Library.

if( WIN32 )
    set( AtlasOnWindows ON )
else( )
    set( AtlasOnWindows OFF )
endif( )

if( APPLE )
    set( AtlasOnApple ON )
else( )
    set( AtlasOnApple OFF )
endif( )

# The OpenGL backend loads its entry points through wglGetProcAddress, so it stays off
# on Apple where Metal is the native backend.
if( APPLE )
    set( AtlasOpenGLDefault OFF )
else( )
    set( AtlasOpenGLDefault ON )
endif( )

# Backends. DirectX is Windows only, Metal is Apple only.
option( ATLAS_DIRECTX11 "Build the DirectX 11 backend" ${AtlasOnWindows} )
option( ATLAS_DIRECTX12 "Build the DirectX 12 backend" ${AtlasOnWindows} )
option( ATLAS_METAL "Build the Metal backend" ${AtlasOnApple} )
option( ATLAS_OPENGL "Build the OpenGL backend" ${AtlasOpenGLDefault} )
option( ATLAS_VULKAN "Build the Vulkan backend" OFF )

option( ATLAS_BUILD_SAMPLE "Build the sample application" ${AtlasOnWindows} )
option( ATLAS_SHARED "Build Atlas as a shared library" OFF )

if( NOT WIN32 AND ( ATLAS_DIRECTX11 OR ATLAS_DIRECTX12 ) )
    message( FATAL_ERROR "ATLAS_DIRECTX11/ATLAS_DIRECTX12 require Windows. Configure with -DATLAS_DIRECTX11=OFF -DATLAS_DIRECTX12=OFF." )
endif( )

if( NOT APPLE AND ATLAS_METAL )
    message( FATAL_ERROR "ATLAS_METAL requires macOS or iOS. Configure with -DATLAS_METAL=OFF." )
endif( )

# Apple's OpenGL is deprecated and caps at 4.1, but this backend needs the 4.3 shader
# storage buffer, and the iOS SDK has no <OpenGL/gl3.h> at all.
if( APPLE AND ATLAS_OPENGL )
    message( FATAL_ERROR "ATLAS_OPENGL is not supported on macOS or iOS, use ATLAS_METAL. Configure with -DATLAS_OPENGL=OFF." )
endif( )

# The renderer-neutral core and the graphics backends are portable, but the platform
# layer only exists for Windows and Apple. Fail here with something readable rather
# than letting an unsupported build collapse into hundreds of compiler errors.
if( NOT WIN32 AND NOT APPLE )
    message( FATAL_ERROR
        "Atlas has a platform layer for Windows and Apple only, and does not support this system.\n"
        "Porting it needs implementations of:\n"
        "  Text/Font/Font.cpp                   glyph rasterising (GDI on Windows, CoreText on Apple)\n"
        "  Ports/Platform/Pictures/Pictures.cpp image decoding (WIC on Windows, ImageIO on Apple)\n"
        "  Ports/Platform/Native/Native.cpp     input, clipboard, cursors (Win32 on Windows, AppKit/UIKit on Apple)\n"
        "  Core/Context/Context.cpp             timing (QueryPerformanceCounter on Windows)\n"
        "Everything else, including the graphics backends, is already portable." )
endif( )

add_subdirectory( Atlas )

if( ATLAS_BUILD_SAMPLE )
    if( NOT WIN32 )
        message( FATAL_ERROR "The sample uses a Win32 host and only builds on Windows. Configure with -DATLAS_BUILD_SAMPLE=OFF." )
    endif( )

    add_subdirectory( Sample )
endif( )
