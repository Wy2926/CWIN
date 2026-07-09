# CWIN.Shell.dll — injected into explorer.exe. Measures the taskbar, reserves
# capsule space, and reports layout to the host. No rendering happens here.
add_library(cwin_shell SHARED
    dllmain.cpp
    TaskbarAdapter.cpp
)
set_target_properties(cwin_shell PROPERTIES OUTPUT_NAME "CWIN.Shell")
target_include_directories(cwin_shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
