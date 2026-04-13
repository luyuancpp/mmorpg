cmake_minimum_required(VERSION 3.22)
project(thread_context)

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
    ../
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
    ../core/
    ./
    /usr/local/include
)

set(SOURCE_FILES
    dispatcher_manager.cpp
    ecs_context.cpp
    entity_manager.cpp
    lua_state.cpp
    message_context.cpp
    node_context_manager.cpp
    player_manager.cpp
    redis_manager.cpp
    registry_manager.cpp
    rpc_manager.cpp
    rpc_request_context.cpp
    snow_flake_manager.cpp
    thread_local_entity_container.cpp
)

add_library(thread_context STATIC ${SOURCE_FILES})

