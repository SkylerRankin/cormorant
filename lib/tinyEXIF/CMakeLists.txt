cmake_minimum_required(VERSION 3.4)
project(tinyEXIF LANGUAGES CXX)

add_library(tinyEXIF STATIC TinyEXIF.cpp include/TinyEXIF.h)
target_link_libraries(tinyEXIF tinyxml2::tinyxml2)
target_include_directories(tinyEXIF PUBLIC include)
