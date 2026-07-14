#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>

#include "Font.h"
#include "Sheet.h"
#include "Format.h"
#include "Context.h"

#pragma comment( lib, "gdi32.lib" )

CFont::~CFont( ) {
    Destroy( );
}

bool CFont::Create( const char* const* Names, int Count, float Height, int Boldness ) {
    Destroy( );

    if ( !Names || Count <= 0 || Height <= 0.0f )
        return false;

    for ( int Position = 0; Position < Count; Position++ )
        Families.push_back( Names[ Position ] );

    BaseHeight = Height;
    Scale = 1.0f;

    Weight = Boldness;
    return Prepare( );
}

void CFont::Destroy( ) {
    Release( );

    Families.clear( );
    Glyphs.clear( );

    for ( int Position = 0; Position < 128; Position++ )
        Quick[ Position ] = nullptr;

    BaseHeight = 0.0f;
    Scale = 1.0f;

    Weight = 400;

    LineSpan = 0.0f;
    Ascent = 0.0f;

    Leading = 0.0f;
}

void CFont::Release( ) {
    if ( Surface ) {
        DeleteDC( ( HDC )Surface );
        Surface = nullptr;
    }

    for ( void* Handle : Handles )
        DeleteObject( ( HFONT )Handle );

    Handles.clear( );
}

void CFont::Rescale( float Factor ) {
    if ( Factor <= 0.0f )
        Factor = 1.0f;

    Scale = Factor;
    Glyphs.clear( );

    for ( int Position = 0; Position < 128; Position++ )
        Quick[ Position ] = nullptr;

    Release( );
    Prepare( );
}

bool CFont::Prepare( ) {
    Release( );

    Surface = CreateCompatibleDC( nullptr );
    if ( !Surface ) {
        Context->Report( "Atlas could not create a font surface" );
        return false;
    }

    int Pixels = ( int )( BaseHeight * Scale + 0.5f );
    if ( Pixels < 4 )
        Pixels = 4;

    for ( const std::string& Family : Families ) {
        HFONT Handle = CreateFontA( -Pixels, 0, 0, 0, Weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, Family.c_str( ) );
        if ( Handle )
            Handles.push_back( Handle );
    }

    if ( Handles.empty( ) ) {
        Context->Report( "Atlas could not open any requested font family" );
        return false;
    }

    SelectObject( ( HDC )Surface, ( HFONT )Handles[ 0 ] );

    TEXTMETRICA Metrics = { };
    GetTextMetricsA( ( HDC )Surface, &Metrics );

    Ascent = ( float )Metrics.tmAscent;
    LineSpan = ( float )( Metrics.tmHeight + Metrics.tmExternalLeading );

    Leading = ( float )Metrics.tmInternalLeading;

    return true;
}

const CGlyph* CFont::Fetch( unsigned int Codepoint ) {
    if ( Codepoint < 128 && Quick[ Codepoint ] )
        return Quick[ Codepoint ];

    auto Existing = Glyphs.find( Codepoint );

    if ( Existing != Glyphs.end( ) ) {
        if ( Codepoint < 128 )
            Quick[ Codepoint ] = &Existing->second;

        return &Existing->second;
    }

    CGlyph Glyph;

    if ( !Rasterize( Codepoint, Glyph ) && Codepoint != '?' ) {
        const CGlyph* Fallback = Fetch( '?' );
        Glyphs[ Codepoint ] = *Fallback;
    } else {
        Glyphs[ Codepoint ] = Glyph;
    }

    const CGlyph* Result = &Glyphs[ Codepoint ];

    if ( Codepoint < 128 )
        Quick[ Codepoint ] = Result;

    return Result;
}

bool CFont::Rasterize( unsigned int Codepoint, CGlyph& Glyph ) {
    if ( !Surface || Handles.empty( ) || Codepoint > 0xFFFF )
        return false;

    HDC Surface2 = ( HDC )Surface;
    wchar_t Wide = ( wchar_t )Codepoint;

    HFONT Chosen = nullptr;

    for ( void* Handle : Handles ) {
        SelectObject( Surface2, ( HFONT )Handle );
        unsigned short Index = 0;

        if ( GetGlyphIndicesW( Surface2, &Wide, 1, &Index, GGI_MARK_NONEXISTING_GLYPHS ) == 1 && Index != 0xFFFF ) {
            Chosen = ( HFONT )Handle;
            break;
        }
    }

    if ( !Chosen )
        return false;

    SelectObject( Surface2, Chosen );

    MAT2 Identity = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    GLYPHMETRICS Outline = { };

    DWORD Needed = GetGlyphOutlineW( Surface2, Codepoint, GGO_GRAY8_BITMAP, &Outline, 0, nullptr, &Identity );
    if ( Needed == GDI_ERROR )
        return false;

    Glyph.Advance = ( float )Outline.gmCellIncX;
    Glyph.Offset = CVector( ( float )Outline.gmptGlyphOrigin.x, Ascent - ( float )Outline.gmptGlyphOrigin.y );

    int Across = ( int )Outline.gmBlackBoxX;
    int Down = ( int )Outline.gmBlackBoxY;

    if ( Needed == 0 || Across <= 0 || Down <= 0 )
        return true;

    std::vector< unsigned char > Scratch( Needed );

    DWORD Copied = GetGlyphOutlineW( Surface2, Codepoint, GGO_GRAY8_BITMAP, &Outline, Needed, Scratch.data( ), &Identity );
    if ( Copied == GDI_ERROR )
        return false;

    int Pitch = ( Across + 3 ) & ~3;
    if ( ( size_t )Pitch * Down > Needed )
        return false;

    int Left = 0;
    int Top = 0;

    if ( !Sheet->Place( Across, Down, Left, Top ) ) {
        Context->Report( "Atlas glyph sheet is full" );
        return false;
    }

    for ( int Row = 0; Row < Down; Row++ ) {
        unsigned char* Line = Sheet->Pixel( Left, Top + Row );

        for ( int Column = 0; Column < Across; Column++ ) {
            int Shade = ( int )Scratch[ ( size_t )Row * Pitch + Column ] * 255 / 64;
            if ( Shade > 255 )
                Shade = 255;

            Line[ Column ] = ( unsigned char )Shade;
        }
    }

    Glyph.Source = CRectangle( ( float )Left, ( float )Top, ( float )Across, ( float )Down );
    Glyph.Span = CVector( ( float )Across, ( float )Down );

    return true;
}

CVector CFont::Measure( const char* Message ) {
    float Reach = 0.0f;

    if ( !Message )
        return CVector( Reach, LineSpan );

    while ( *Message )
        Reach += Fetch( DecodeCharacter( Message ) )->Advance;

    return CVector( Reach, LineSpan );
}

float CFont::Span( const char* Message, int Bytes ) {
    if ( !Message || Bytes <= 0 )
        return 0.0f;

    float Reach = 0.0f;

    const char* Cursor = Message;
    const char* Limit = Message + Bytes;

    while ( *Cursor && Cursor < Limit )
        Reach += Fetch( DecodeCharacter( Cursor ) )->Advance;

    return Reach;
}

int CFont::Hit( const char* Message, float Across ) {
    if ( !Message )
        return 0;

    float Reach = 0.0f;
    const char* Cursor = Message;

    while ( *Cursor ) {
        const char* Before = Cursor;
        float Advance = Fetch( DecodeCharacter( Cursor ) )->Advance;

        if ( Across < Reach + Advance * 0.5f )
            return ( int )( Before - Message );

        Reach += Advance;
    }

    return ( int )( Cursor - Message );
}