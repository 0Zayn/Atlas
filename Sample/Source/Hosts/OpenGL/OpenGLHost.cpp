#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <gl/GL.h>

#include "OpenGL.h"
#include "OpenGLHost.h"

bool COpenGLHost::Create( void* Window, int Width, int Height ) {
    Destroy( );

    Handle = Window;
    Surface = GetDC( ( HWND )Window );

    if ( !Surface )
        return false;

    HDC Paper = ( HDC )Surface;
    PIXELFORMATDESCRIPTOR Recipe = { };

    Recipe.nSize = sizeof( PIXELFORMATDESCRIPTOR );
    Recipe.nVersion = 1;

    Recipe.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    Recipe.iPixelType = PFD_TYPE_RGBA;

    Recipe.cColorBits = 32;
    Recipe.iLayerType = PFD_MAIN_PLANE;

    if ( GetPixelFormat( Paper ) == 0 ) {
        int Chosen = ChoosePixelFormat( Paper, &Recipe );

        if ( !Chosen || !SetPixelFormat( Paper, Chosen, &Recipe ) ) {
            Destroy( );
            return false;
        }
    }

    HGLRC Legacy = wglCreateContext( Paper );
    if ( !Legacy || !wglMakeCurrent( Paper, Legacy ) ) {
        Destroy( );
        return false;
    }

    HGLRC( WINAPI * CreateModern )( HDC, HGLRC, const int* ) = ( HGLRC( WINAPI* )( HDC, HGLRC, const int* ) )wglGetProcAddress( "wglCreateContextAttribsARB" );

    if ( CreateModern ) {
        int Wishes[ 7 ] = { 0x2091, 4, 0x2092, 3, 0x9126, 0x0001, 0 };
        HGLRC Modern = CreateModern( Paper, nullptr, Wishes );

        if ( Modern ) {
            wglMakeCurrent( Paper, Modern );
            wglDeleteContext( Legacy );

            Setting = Modern;
        } else {
            Setting = Legacy;
        }
    } else {
        Setting = Legacy;
    }

    SurfaceWidth = Width;
    SurfaceHeight = Height;

    Interval = -1;

    if ( !OpenGL->Create( ) ) {
        Destroy( );
        return false;
    }

    return true;
}

void COpenGLHost::Destroy( ) {
    if ( Setting ) {
        OpenGL->Destroy( );
        wglMakeCurrent( nullptr, nullptr );

        wglDeleteContext( ( HGLRC )Setting );
        Setting = nullptr;
    }

    if ( Surface && Handle )
        ReleaseDC( ( HWND )Handle, ( HDC )Surface );

    Surface = nullptr;
    Handle = nullptr;

    SurfaceWidth = 0;
    SurfaceHeight = 0;

    Interval = -1;
}

void COpenGLHost::Resize( int Width, int Height ) {
    if ( Width > 0 && Height > 0 ) {
        SurfaceWidth = Width;
        SurfaceHeight = Height;
    }
}

void COpenGLHost::Begin( CColor Backdrop ) {
    if ( !Setting )
        return;

    glViewport( 0, 0, SurfaceWidth, SurfaceHeight );

    glClearColor( Backdrop.Red / 255.0f, Backdrop.Green / 255.0f, Backdrop.Blue / 255.0f, Backdrop.Alpha / 255.0f );
    glClear( GL_COLOR_BUFFER_BIT );
}

void* COpenGLHost::Stream( ) {
    return nullptr;
}

CGraphics* COpenGLHost::Graphics( ) {
    return OpenGL.get( );
}

void COpenGLHost::End( bool VerticalSync ) {
    if ( !Setting || !Surface )
        return;

    int Wanted = VerticalSync ? 1 : 0;

    if ( Wanted != Interval ) {
        BOOL( WINAPI * SwapPace )( int ) = ( BOOL( WINAPI* )( int ) )wglGetProcAddress( "wglSwapIntervalEXT" );

        if ( SwapPace )
            SwapPace( Wanted );

        Interval = Wanted;
    }

    SwapBuffers( ( HDC )Surface );
}