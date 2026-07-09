add_library(cwin_common STATIC
    Config.cpp
)
target_include_directories(cwin_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
