#set the minimum required version of cmake
cmake_minimum_required(VERSION 3.10)

#set the project name
project(bst-http)

#set the C++ standard to use
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable coroutines
if(MSVC)
    add_compile_options(/await)
else()
    add_compile_options(-fcoroutines)
endif()

#check dependencies
find_package(Boost 1.74 REQUIRED)

#set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

#include headers
include_directories(http)

#install headers
install(DIRECTORY http/ DESTINATION /usr/local/include/bst-http)