add_library(protobuf_codec codec.cc)

if(MSVC)
LINK_DIRECTORIES("${protobuf_BINARY_DIR}/Debug")
LINK_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}/Debug")
endif (MSVC)

if (MSVC)
target_link_libraries(protobuf_codec libprotobufd muduo_net zlibstatic)
else(MSVC)
target_link_libraries(protobuf_codec protobuf muduo_net z)
endif()

add_custom_command(OUTPUT query.pb.cc query.pb.h
  COMMAND protoc
  ARGS --cpp_out . ${CMAKE_CURRENT_SOURCE_DIR}/query.proto -I${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS query.proto
  VERBATIM )

if (NOT MSVC)
set_source_files_properties(query.pb.cc PROPERTIES COMPILE_FLAGS "-Wno-conversion -Wno-shadow")
endif()
include_directories(${PROJECT_BINARY_DIR})

add_library(query_proto query.pb.cc)
if (MSVC)
target_link_libraries(query_proto libprotobufd muduo_net libprotobuf-lited zlibstatic)
else(MSVC)
target_link_libraries(query_proto protobuf pthread)
endif()

add_executable(protobuf_codec_test codec_test.cc)
if (NOT MSVC)
set_target_properties(protobuf_codec_test PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()
target_link_libraries(protobuf_codec_test protobuf_codec query_proto)

add_executable(protobuf_dispatcher_lite_test dispatcher_lite_test.cc)
if (NOT MSVC)
set_target_properties(protobuf_dispatcher_lite_test PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()
target_link_libraries(protobuf_dispatcher_lite_test query_proto)

add_executable(protobuf_dispatcher_test dispatcher_test.cc)
if (NOT MSVC)
set_target_properties(protobuf_dispatcher_test PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()
target_link_libraries(protobuf_dispatcher_test query_proto)

add_executable(protobuf_server server.cc)
if (NOT MSVC)
set_target_properties(protobuf_server PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()
target_link_libraries(protobuf_server protobuf_codec query_proto)

add_executable(protobuf_client client.cc)
if (NOT MSVC)
set_target_properties(protobuf_client PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()
target_link_libraries(protobuf_client protobuf_codec query_proto)

add_custom_target(protobuf_codec_all
                  DEPENDS
                        protobuf_codec_test
                        protobuf_dispatcher_lite_test
                        protobuf_dispatcher_test
                        protobuf_server
                        protobuf_client)
