add_library(cwin_common STATIC
    Config.cpp
    Log.cpp
)
target_include_directories(cwin_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cwin_common PUBLIC shell32 ole32)
