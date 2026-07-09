add_library(cwin_providers STATIC
    Providers.cpp
)
target_include_directories(cwin_providers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cwin_providers PUBLIC cwin_common pdh iphlpapi winhttp)
