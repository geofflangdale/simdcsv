
cmake_minimum_required(VERSION 2.8)

project(simdcsv)

if (NOT CMAKE_BUILD_TYPE)
                message(STATUS "No build type selected, default to Release")
                set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)

include_directories("${PROJECT_SOURCE_DIR}/src")
#include_directories("${PROJECT_SOURCE_DIR}/thirdparty")

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

set(CMAKE_CXX_FLAGS                  "-std=c++17 -march=native -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO   "-O3 -g")
set(CMAKE_CXX_FLAGS_RELEASE          "-O3")
set(CMAKE_CXX_FLAGS_DEBUG            "-DDEBUG -O0 -g")

# evidently useful:
macro(append var string)
  set(${var} "${${var}} ${string}")
endmacro(append)

# To build with sanitizers, do cmake  -DSIMDCSV_SANITIZE=ON ..
if(SIMDCSV_SANITIZE)
  set(SIMDCSV_SANITIZE_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover=all")
  if (CMAKE_COMPILER_IS_GNUCC)
    # Ubuntu bug for GCC 5.0+ (safe for all versions)
    append(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=gold")
    append(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=gold")
  endif()
  append(CMAKE_CXX_FLAGS ${SIMDCSV_SANITIZE_FLAGS})
  MESSAGE( STATUS "Sanitizers requested.")
endif()

add_executable(simdcsv ${SOURCES})

# Are you sure you know the settings? Let us print them out:
MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

MESSAGE( STATUS "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER} ) # important to know which compiler is used
MESSAGE( STATUS "CMAKE_CXX_FLAGS: " ${CMAKE_CXX_FLAGS} ) # important to know the flags
MESSAGE( STATUS "CMAKE_CXX_FLAGS_DEBUG: " ${CMAKE_CXX_FLAGS_DEBUG} )
MESSAGE( STATUS "CMAKE_CXX_FLAGS_RELEASE: " ${CMAKE_CXX_FLAGS_RELEASE} )
