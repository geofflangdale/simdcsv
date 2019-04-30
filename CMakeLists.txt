
cmake_minimum_required(VERSION 2.8)

project(simdcsv)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)

include_directories("${PROJECT_SOURCE_DIR}/src")
#include_directories("${PROJECT_SOURCE_DIR}/thirdparty")

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

set(CMAKE_CXX_FLAGS                  "-std=c++17 -march=native -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO   "-O3 -g")
set(CMAKE_CXX_FLAGS_RELEASE          "-O3")
set(CMAKE_CXX_FLAGS_DEBUG            "-DDEBUG -O0 -g")

add_executable(simdcsv ${SOURCES})
