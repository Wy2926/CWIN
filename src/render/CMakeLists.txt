add_library(cwin_render STATIC
    Renderer.cpp
    Backdrop.cpp
)
target_include_directories(cwin_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cwin_render PUBLIC
    cwin_common
    d2d1 dwrite dcomp d3d11 dxgi dwmapi
)
