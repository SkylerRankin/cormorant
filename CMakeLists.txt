cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(cormorant LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lib)
add_subdirectory(src)
