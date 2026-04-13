cmake_minimum_required(VERSION 3.22)
project(config)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-sign-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fPIC")

set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib")

add_definitions(-D__LINUX__)
add_definitions(-D__linux__)
add_definitions(-DNOMINMAX)
add_definitions(-DENTT_ID_TYPE=uint64_t)
add_definitions(-DABSL_PROPAGATE_CXX_STD=TRUE)

include_directories(
    ../../
    ../../../../third_party/
    ../../../../third_party/boost/
    ../../../../third_party/librdkafka/src-cpp/
    ../../../../third_party/lua/
    ../../../../third_party/lua/src/
    ../../../../third_party/muduo-linux/
    ../../../../third_party/redis/
    ../../../../third_party/redis/deps/
    ../../../../third_party/sol2/include/
    ../../../../third_party/spdlog/include/
    ../../../../third_party/xxhash/
    ../../../../third_party/yaml-cpp/include/
    ../../../../third_party/zlib/
    ../../../generated/
    ./
    /usr/local/include
)

set(SOURCE_FILES
    config.cpp
    node_config_manager.cpp
)

add_library(config STATIC ${SOURCE_FILES})

