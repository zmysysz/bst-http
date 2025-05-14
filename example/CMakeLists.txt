#set the minimum required version of cmake
cmake_minimum_required(VERSION 3.10)

#set the project name
project(bst-http-example)

#set the C++ standard to use
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#check dependencies
find_package(Boost 1.74 REQUIRED)

#set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

#include headers
include_directories(/usr/local/include)

#set executable
add_executable(example example.cpp)

#add link librarie files
target_link_libraries(example libboost_coroutine.so libboost_context.so libboost_url.so)
