#include "Style.h"

void CStyle::Dark( ) {
    Preset = 0;

    Backdrop = CColor( 10, 10, 13 );
    Surface = CColor( 21, 21, 26 );

    Elevated = CColor( 30, 30, 37 );
    Header = CColor( 26, 26, 32 );

    Outline = CColor( 255, 255, 255, 20 );
    Highlight = CColor( 255, 255, 255, 16 );

    Text = CColor( 237, 238, 243 );
    Faint = CColor( 138, 141, 153 );

    Accent = CColor( 74, 124, 255 );
    AccentSoft = CColor( 130, 166, 255 );

    Control = CColor( 32, 32, 39 );
    Selected = CColor( 32, 42, 66 );

    Hovered = CColor( 43, 43, 52 );
    Pressed = CColor( 26, 26, 32 );

    Groove = CColor( 42, 42, 51 );
    Knob = CColor( 240, 241, 247 );

    Shade = CColor( 0, 0, 0, 140 );
    Focus = CColor( 74, 124, 255 );

    Tab = CColor( 16, 16, 20 );
    TabActive = CColor( 30, 30, 37 );

    Popup = CColor( 28, 28, 34 );
    Danger = CColor( 240, 92, 104 );

    ScrollTrack = CColor( 30, 30, 37, 150 );
    ScrollThumb = CColor( 62, 62, 74 );

    Apply( );
}

void CStyle::Light( ) {
    Preset = 1;

    Backdrop = CColor( 239, 240, 245 );
    Surface = CColor( 253, 253, 255 );

    Elevated = CColor( 247, 248, 251 );
    Header = CColor( 248, 249, 252 );

    Outline = CColor( 17, 20, 34, 26 );
    Highlight = CColor( 255, 255, 255, 130 );

    Text = CColor( 26, 28, 38 );
    Faint = CColor( 120, 124, 139 );

    Accent = CColor( 48, 108, 244 );
    AccentSoft = CColor( 130, 166, 255 );

    Control = CColor( 244, 245, 249 );
    Selected = CColor( 224, 233, 255 );

    Hovered = CColor( 235, 237, 243 );
    Pressed = CColor( 225, 227, 235 );

    Groove = CColor( 224, 226, 234 );
    Knob = CColor( 255, 255, 255 );

    Shade = CColor( 28, 32, 58, 40 );
    Focus = CColor( 48, 108, 244 );

    Tab = CColor( 232, 234, 241 );
    TabActive = CColor( 255, 255, 255 );

    Popup = CColor( 252, 252, 254 );
    Danger = CColor( 224, 66, 78 );

    ScrollTrack = CColor( 225, 227, 235, 150 );
    ScrollThumb = CColor( 178, 182, 196 );

    Apply( );
}

void CStyle::Apply( ) {
    Rounding = 16.0f * Scale;
    ControlRounding = 10.0f * Scale;

    PaddingWide = 20.0f * Scale;
    PaddingTall = 18.0f * Scale;

    Spacing = 13.0f * Scale;
    Thickness = 1.0f * Scale;

    Softness = 26.0f * Scale;
    Glow = 22.0f * Scale;

    TitleHeight = 46.0f * Scale;
    ControlHeight = 34.0f * Scale;

    TabHeight = 38.0f * Scale;
    SplitterSize = 5.0f * Scale;

    GripSize = 18.0f * Scale;
    CheckSize = 20.0f * Scale;

    KnobSize = 7.0f * Scale;
    GrooveWidth = 4.0f * Scale;

    Underline = 2.0f * Scale;
    IconStroke = 1.7f * Scale;

    FadeSpeed = 14.0f;
}

void CStyle::Rescale( float Factor ) {
    if ( Factor <= 0.0f )
        Factor = 1.0f;

    Scale = Factor;

    if ( Preset == 1 ) {
        Light( );
        return;
    }

    Dark( );
}