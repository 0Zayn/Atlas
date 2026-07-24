set( AtlasRoot ${CMAKE_CURRENT_SOURCE_DIR}/Source )

set( AtlasSources
    ${AtlasRoot}/Core/Arena/Arena.cpp
    ${AtlasRoot}/Core/Context/Context.cpp
    ${AtlasRoot}/Core/Input/Input.cpp
    ${AtlasRoot}/Text/Font/Font.cpp
    ${AtlasRoot}/Text/Format/Format.cpp
    ${AtlasRoot}/Text/Sheet/Sheet.cpp
    ${AtlasRoot}/Interface/Layout/Layout.cpp
    ${AtlasRoot}/Interface/Style/Style.cpp
    ${AtlasRoot}/Interface/Widgets/Widgets.cpp
    ${AtlasRoot}/Interface/Desktop/Docking/Docking.cpp
    ${AtlasRoot}/Interface/Desktop/Frames/Frames.cpp
    ${AtlasRoot}/Ports/Canvas/Canvas.cpp
    ${AtlasRoot}/Ports/Shaders/Shaders.cpp
)

if( WIN32 )
    list( APPEND AtlasSources
        ${AtlasRoot}/Ports/Platform/Native/Native.cpp
        ${AtlasRoot}/Ports/Platform/Pictures/Pictures.cpp
    )
elseif( APPLE )
    # AppKit and UIKit are Objective-C, so the platform leaves are built as Objective-C++.
    # A leaf may ship either an .mm or a .cpp that needs the language forcing.
    foreach( AtlasLeaf Native Pictures )
        if( EXISTS ${AtlasRoot}/Ports/Platform/${AtlasLeaf}/${AtlasLeaf}.mm )
            list( APPEND AtlasSources ${AtlasRoot}/Ports/Platform/${AtlasLeaf}/${AtlasLeaf}.mm )
        else( )
            list( APPEND AtlasSources ${AtlasRoot}/Ports/Platform/${AtlasLeaf}/${AtlasLeaf}.cpp )
            set_source_files_properties( ${AtlasRoot}/Ports/Platform/${AtlasLeaf}/${AtlasLeaf}.cpp PROPERTIES LANGUAGE OBJCXX )
        endif( )
    endforeach( )
endif( )

if( ATLAS_DIRECTX11 )
    list( APPEND AtlasSources ${AtlasRoot}/Ports/Graphics/DirectX11/DirectX11.cpp )
endif( )

if( ATLAS_DIRECTX12 )
    list( APPEND AtlasSources ${AtlasRoot}/Ports/Graphics/DirectX12/DirectX12.cpp )
endif( )

if( ATLAS_METAL )
    list( APPEND AtlasSources ${AtlasRoot}/Ports/Graphics/Metal/Metal.mm )

    # The Metal port stores Objective-C objects as C++ members and never retains or
    # releases them, so it requires automatic reference counting.
    set_source_files_properties( ${AtlasRoot}/Ports/Graphics/Metal/Metal.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc" )
endif( )

if( ATLAS_OPENGL )
    list( APPEND AtlasSources ${AtlasRoot}/Ports/Graphics/OpenGL/OpenGL.cpp )
endif( )

if( ATLAS_VULKAN )
    list( APPEND AtlasSources ${AtlasRoot}/Ports/Graphics/Vulkan/Vulkan.cpp )
endif( )

if( ATLAS_SHARED )
    add_library( Atlas SHARED ${AtlasSources} )
else( )
    add_library( Atlas STATIC ${AtlasSources} )
endif( )

add_library( Atlas::Atlas ALIAS Atlas )

# Short "staircase" includes: every leaf folder is an include directory.
# Never add Ports/Graphics itself, its Vulkan folder shadows <vulkan/vulkan.h>.
target_include_directories( Atlas PUBLIC
    $<BUILD_INTERFACE:${AtlasRoot}>
    $<BUILD_INTERFACE:${AtlasRoot}/Core>
    $<BUILD_INTERFACE:${AtlasRoot}/Core/Arena>
    $<BUILD_INTERFACE:${AtlasRoot}/Core/Context>
    $<BUILD_INTERFACE:${AtlasRoot}/Core/Input>
    $<BUILD_INTERFACE:${AtlasRoot}/Text/Font>
    $<BUILD_INTERFACE:${AtlasRoot}/Text/Format>
    $<BUILD_INTERFACE:${AtlasRoot}/Text/Sheet>
    $<BUILD_INTERFACE:${AtlasRoot}/Interface/Layout>
    $<BUILD_INTERFACE:${AtlasRoot}/Interface/Style>
    $<BUILD_INTERFACE:${AtlasRoot}/Interface/Widgets>
    $<BUILD_INTERFACE:${AtlasRoot}/Interface/Desktop/Docking>
    $<BUILD_INTERFACE:${AtlasRoot}/Interface/Desktop/Frames>
    $<BUILD_INTERFACE:${AtlasRoot}/Ports/Canvas>
    $<BUILD_INTERFACE:${AtlasRoot}/Ports/Shaders>
    $<BUILD_INTERFACE:${AtlasRoot}/Ports/Platform/Native>
    $<BUILD_INTERFACE:${AtlasRoot}/Ports/Platform/Pictures>
    $<BUILD_INTERFACE:${AtlasRoot}/Ports/Graphics/DirectX11>
    $<BUILD_INTERFACE:${AtlasRoot}/Ports/Graphics/DirectX12>
    $<BUILD_INTERFACE:${AtlasRoot}/Ports/Graphics/OpenGL>
    $<BUILD_INTERFACE:${AtlasRoot}/Ports/Graphics/Vulkan>
    $<INSTALL_INTERFACE:include>
)

if( APPLE )
    target_include_directories( Atlas PUBLIC $<BUILD_INTERFACE:${AtlasRoot}/Ports/Graphics/Metal> )
endif( )

if( MSVC )
    target_compile_options( Atlas PRIVATE /W4 /utf-8 )
else( )
    target_compile_options( Atlas PRIVATE -Wall -Wextra )
endif( )

if( WIN32 )
    target_link_libraries( Atlas PUBLIC user32 gdi32 ole32 windowscodecs d2d1 )

    if( ATLAS_DIRECTX11 )
        target_link_libraries( Atlas PUBLIC d3d11 d3dcompiler dxgi )
    endif( )

    if( ATLAS_DIRECTX12 )
        target_link_libraries( Atlas PUBLIC d3d12 d3dcompiler dxgi )
    endif( )

    if( ATLAS_OPENGL )
        target_link_libraries( Atlas PUBLIC opengl32 )
    endif( )
else( )
    find_package( Threads REQUIRED )
    target_link_libraries( Atlas PUBLIC Threads::Threads ${CMAKE_DL_LIBS} )

    if( ATLAS_OPENGL )
        find_package( OpenGL REQUIRED )
        target_link_libraries( Atlas PUBLIC OpenGL::GL )
    endif( )
endif( )

if( APPLE )
    set( AtlasFrameworks Foundation QuartzCore CoreGraphics CoreText ImageIO )

    if( CMAKE_SYSTEM_NAME STREQUAL "iOS" )
        list( APPEND AtlasFrameworks UIKit )
    else( )
        list( APPEND AtlasFrameworks AppKit )
    endif( )

    if( ATLAS_METAL )
        list( APPEND AtlasFrameworks Metal MetalKit )

        target_compile_definitions( Atlas PUBLIC ATLAS_METAL=1 )
    else( )
        target_compile_definitions( Atlas PUBLIC ATLAS_METAL=0 )
    endif( )

    foreach( AtlasFramework IN LISTS AtlasFrameworks )
        find_library( AtlasFound${AtlasFramework} ${AtlasFramework} REQUIRED )
        target_link_libraries( Atlas PUBLIC ${AtlasFound${AtlasFramework}} )
    endforeach( )
endif( )

if( ATLAS_VULKAN )
    find_package( Vulkan REQUIRED )

    target_compile_definitions( Atlas PUBLIC ATLAS_VULKAN=1 )
    target_link_libraries( Atlas PUBLIC Vulkan::Vulkan )
endif( )

install( TARGETS Atlas EXPORT AtlasTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin )
install( DIRECTORY ${AtlasRoot}/ DESTINATION include FILES_MATCHING PATTERN "*.h" )
