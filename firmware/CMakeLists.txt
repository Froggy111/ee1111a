cmake_minimum_required(VERSION 4.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PICO_SDK_FETCH_FROM_GIT ON)
include(pico_sdk_import.cmake)

project(fw)

pico_sdk_init()

add_subdirectory(src)
