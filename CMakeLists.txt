cmake_minimum_required(VERSION 3.15)
project(spock)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# deps 
find_package(Vulkan REQUIRED)
include(FetchContent)

# utils 
include(cmake/FindSlang.cmake)
include(cmake/SlangUtils.cmake)

add_subdirectory(ext)

add_subdirectory(lib)


