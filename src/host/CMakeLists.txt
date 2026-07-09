add_executable(cwin_host WIN32
    main.cpp
    HostWindow.cpp
)
set_target_properties(cwin_host PROPERTIES OUTPUT_NAME "CWIN.Host")
target_include_directories(cwin_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cwin_host PRIVATE
    cwin_common cwin_render cwin_providers cwin_ipc
)
