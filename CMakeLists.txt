cmake_minimum_required(VERSION 3.21)
project(CWIN VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
    message(FATAL_ERROR "CWIN requires MSVC on Windows")
endif()

add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
add_compile_options(/W4 /permissive- /utf-8)

add_subdirectory(src/common)
add_subdirectory(src/render)
add_subdirectory(src/providers)
add_subdirectory(src/ipc)
add_subdirectory(src/shell)
add_subdirectory(src/host)
