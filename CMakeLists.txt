cmake_minimum_required(VERSION 3.15)
project(spock)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# deps 
find_package(Vulkan REQUIRED)
include(FetchContent)

add_subdirectory(ext)

add_subdirectory(lib)


