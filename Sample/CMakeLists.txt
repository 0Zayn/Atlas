set( SampleRoot ${CMAKE_CURRENT_SOURCE_DIR}/Source )

set( SampleSources
    ${SampleRoot}/EntryPoint.cpp
    ${SampleRoot}/Hosts/DirectX11/ElevenHost.cpp
    ${SampleRoot}/Hosts/DirectX12/TwelveHost.cpp
    ${SampleRoot}/Hosts/OpenGL/OpenGLHost.cpp
    ${SampleRoot}/Hosts/Vulkan/VulkanHost.cpp
)

add_executable( Sample WIN32 ${SampleSources} )

target_include_directories( Sample PRIVATE
    ${SampleRoot}/Hosts
    ${SampleRoot}/Hosts/DirectX11
    ${SampleRoot}/Hosts/DirectX12
    ${SampleRoot}/Hosts/OpenGL
    ${SampleRoot}/Hosts/Vulkan
)

target_link_libraries( Sample PRIVATE Atlas::Atlas winmm dwmapi )

if( MSVC )
    target_compile_options( Sample PRIVATE /W4 /utf-8 )
endif( )

set_target_properties( Sample PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:Sample> )
