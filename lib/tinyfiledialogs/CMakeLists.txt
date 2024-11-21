cmake_minimum_required(VERSION 3.4)
project(tinyfiledialogs LANGUAGES CXX)

add_library(tinyfiledialogs STATIC tinyfiledialogs.cpp)
target_include_directories(tinyfiledialogs PUBLIC include)
