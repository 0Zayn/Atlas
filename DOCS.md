# Atlas Documentation

A reference for building UIs with Atlas. Everything is exposed through a handful of global objects (`Atlas`, `Frames`, `Widgets`, `Layout`, `Style`, `Docking`, `Canvas`, `Context`, `Input`).

## Architecture

Atlas is a UI library, not a windowing framework. The split is:

- **Your host owns**: the window, the graphics device, the swapchain, and the renderer.
- **Atlas owns**: widget state, layout, input routing, and turning it all into draw data, then rendering that draw data through a backend port you give a device/context to.

Each backend has the `CGraphics` interface. The `Sample/Source/Hosts/` folder has a complete wrapper for DirectX 11, DirectX 12, OpenGL, and Vulkan, you can copy the ones you need.

## Frame loop

```cpp
Atlas->Create( ); /* After the device is ready */
Atlas->Rescale( DpiScale ); /* Optional, on DPI change */

/* Every frame */
Atlas->Begin( CVector( Width, Height ) );
/* Create windows and widgets here */
Atlas->End( );

Host->Begin( Style->Backdrop );
Host->Graphics( )->Render( Atlas->Data( ), Host->Stream( ) );
Host->End( VerticalSync );

Atlas->Destroy( ); /* On shutdown */
```

Feed input from your window proc `Ports/Platform/Native` translates Win32 messages into Atlas input (`Native->Translate(...)`).

## Windows

```cpp
if ( Frames->Begin( "Title", &Open, FrameDefault, Anchor, Size ) ) {
    /* Widgets */
}
Frames->End( );
```

Options are flags: `FrameMove`, `FrameResize`, `FrameCollapse`, `FrameClose`, `FrameDock` (`FrameDefault` = all). Pass a `bool*` for a close button. `Atlas->Diagnostics(&Open)` opens a built-in FPS/draw-call panel.

## Widgets

Text: `Label`, `Faint`, `Heading`, `Section`, `Wrapped`, `Bullet`, `Colored`.

Buttons: `Button`, `Small`.

Booleans: `Check`, `Toggle`, `Radio`.

Numbers: `Slider`, `SliderWhole`, `Drag`, `DragWhole`, `Vector`, `Number`, `Decimal`.

Text input: `Field` (single line), `Area` (multi-line — selection, clipboard, undo/redo).

Selection: `Choice` (dropdown), `Segments` (segmented control), `List`, `Selectable`.

Hierarchy: `Tree` / `TreeLeaf` / `TreePop`, `Collapsing`, `BeginCollapse` / `EndCollapse`.

Tabs & menus: `BeginTabs` / `Tab` / `EndTabs`, `BeginMenuBar` / `BeginMenu` / `MenuItem` / `EndMenu` / `EndMenuBar`, `OpenPopup` / `BeginPopup` / `EndPopup`.

Data: `Plot`, `Histogram`, `Area`, `Pie`.

Other: `Color` (HSV picker), `Picture`, `Progress`, `Separator`, `Tooltip`.

Most interactive widgets return `bool` (changed/clicked) and take state by reference:

```cpp
if ( Widgets->Check( "Enabled", Enabled ) ) { /* Toggled */ }
Widgets->Slider( "Gain", Gain, 0.0f, 1.0f );

const char* Items[] = { "Low", "Medium", "High" };
Widgets->Choice( "Quality", Index, Items, 3 );
```

Append `##id` to a label to keep a unique ID while showing the same text (or hide text after it).

## Layout

Widgets stack vertically by default. Control flow with:

- `Layout->SameLine()` — put the next widget on the same row.
- `Layout->PushWidth(w)` / `PopWidth()` — set item width.
- `Layout->Indent()` / `Unindent()`, `Skip(px)`, `Spacing()`, `Dummy(size)`.
- `Layout->BeginGroup()` / `EndGroup()`.
- `Layout->BeginChild(id, size)` / `EndChild()` — a scrolling sub-region.

Tables with resizable columns:

```cpp
if ( Layout->BeginTable( "Grid", 3 ) ) {
    Layout->TableSetup( "Name" );
    Layout->TableSetup( "Value", 90.0f );
    Layout->TableSetup( "State" );
    Layout->TableHeaders( );

    for ( ... ) {
        Layout->TableRow( );
        Layout->TableColumn( ); 

        Widgets->Label( Name );
        Layout->TableColumn( );

        Widgets->Label( Value );
        Layout->TableColumn( ); 

        Widgets->Faint( State );
    }
    Layout->EndTable( );
}
```

## Docking

Docking is on by default (`Docking->Enabled`). Give it a region once per frame and Atlas handles the rest, drag a window's title bar over another window to see anchor pills (center = tab, edges = split, corners = quadrant), tabbed islands, and splitters:

```cpp
Docking->Space( CRectangle( 0, 0, Width, Height ) ); /* Before your windows */
```

## Styling

```cpp
Style->Dark( ); /* Or Style->Light( ) */
Style->Accent = CColor( 74, 124, 255 );
Style->Rounding = 16.0f;
```

Toggles: `Style->Shadows`, `Style->Borders`, `Style->Glass`, `Style->ScrollSmooth`, `Style->Adaptive`. Metrics (padding, heights, rounding, `FadeSpeed`, …) are all fields — never hardcode; read them so your UI scales with DPI via `Atlas->Rescale`.

## Custom rendering & shaders

Drop native draw commands (a mesh, a scene, or justt about anything) into the UI:

```cpp
Canvas->Custom( Bounds, RenderCallback, Data ); /* Callback gets the backend's command stream */
```

Write one effect and Atlas compiles it for HLSL and GLSL, no more wasting time and effort

```cpp
unsigned int Effect = Shaders->Compose( "Bloom", BloomBodySource );
unsigned int Former = Canvas->Effect( Effect );

Widgets->Picture( Image, Size ); /* Drawn with the effect */
Canvas->Effect( Former );
```

The Sample renders a 3D scene this way, portable across all four backends.

## Persistence

```cpp
Atlas->Save( "atlas.layout" ); /* Window + dock layout */
Atlas->Load( "atlas.layout" );
```

Layout loading is checked a corrupt or outdated file resets to defaults instead of crashing.

## Error handling

Backends, fonts, and image loading fail gracefully and are reported through `Context->Report(...)`, which build up in `Context->Faults` (and mirrored to the debugger). Set `Context->Alarm` for a callback.