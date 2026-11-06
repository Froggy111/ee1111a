cmake_minimum_required(VERSION 4.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_SDK_FETCH_FROM_GIT ON)
include(pico_sdk_import.cmake)

project(fw)

pico_sdk_init()

add_subdirectory(src)
