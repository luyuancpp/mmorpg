cmake_minimum_required(VERSION 3.2.0)

if(POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()

project(muduo C CXX)

include(DownloadProject.cmake)

download_third_party(boost https://dl.bintray.com/boostorg/release/1.75.0/source/boost_1_75_0.zip)
download_third_party(googletest https://codeload.github.com/google/googletest/zip/master)
download_third_party(protobuf https://github.com/protocolbuffers/protobuf/releases/download/v3.15.6/protobuf-all-3.15.6.zip)
download_third_party(zlib https://github.com/madler/zlib/archive/refs/tags/v1.2.11.zip)

# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

enable_testing()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if (MSVC)

  # Build with multiple processes
  add_definitions(/MP)
  add_definitions( /wd4068 /wd4996 /wd4244)
  # Allow big object
  add_definitions(/bigobj)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ")
  set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
    foreach(CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()

else (MSVC)

  set(CXX_FLAGS
  -g
  # -DVALGRIND
  # -DMUDUO_STD_STRING
  -DCHECK_PTHREAD_RETURN_VALUE
  -D_FILE_OFFSET_BITS=64
  -Wall
  -Wextra
  -Werror
  -Wconversion
  -Wno-unused-parameter
  #-Wold-style-cast
  -Woverloaded-virtual
  -Wpointer-arith
  -Wshadow
  -Wwrite-strings
  -march=native
  -rdynamic
  -std=c++11
  -pthread 
  -lpthread 
  --std=c++11 
  -D_GLIBCXX_USE_DEPRECATED=0
  )
  
if(CMAKE_BUILD_BITS EQUAL 64)
  list(APPEND CXX_FLAGS "-m64")
endif()

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -finline-limit=1000 -DNDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
endif (MSVC)

include_directories(contrib/boost/boost)
include_directories(contrib/windows/include)
include_directories(boost)
include_directories(gtest/googletest/include)
include_directories(gtest/googlemock/include)
include_directories(protobuf/src)
include_directories(muduo/net/protorpc)
include_directories(zlib)
include_directories("${zlib_BINARY_DIR}")

include_directories(${PROJECT_SOURCE_DIR})

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
message(STATUS "CXX_FLAGS = " ${CMAKE_CXX_FLAGS} " " ${CMAKE_CXX_FLAGS_${BUILD_TYPE}})

add_subdirectory(googletest)
add_subdirectory(protobuf/cmake)
add_subdirectory(muduo/base)
add_subdirectory(muduo/net)
add_subdirectory(examples/protobuf)
add_subdirectory(zlib)
