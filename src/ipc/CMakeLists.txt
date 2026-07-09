add_library(cwin_ipc STATIC
    IpcServer.cpp
    IpcClient.cpp
    IpcProtocol.cpp
)
target_include_directories(cwin_ipc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cwin_ipc PUBLIC cwin_common)
