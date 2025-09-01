if(MSVC)
LINK_DIRECTORIES("${protobuf_BINARY_DIR}/Debug")
LINK_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}/Debug")
endif (MSVC)

add_custom_command(OUTPUT rpc.pb.cc rpc.pb.h
  COMMAND protoc
  ARGS --cpp_out . ${CMAKE_CURRENT_SOURCE_DIR}/rpc.proto -I${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS rpc.proto
  VERBATIM )

if (NOT MSVC)
set_source_files_properties(rpc.pb.cc PROPERTIES COMPILE_FLAGS "-Wno-conversion")
endif()
include_directories(${PROJECT_BINARY_DIR})

add_library(muduo_protorpc_wire rpc.pb.cc RpcCodec.cc)
if (NOT MSVC)
set_target_properties(muduo_protorpc_wire PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()

add_library(muduo_protorpc_wire_cpp11 rpc.pb.cc RpcCodec.cc)

if (UNIX)
 set_target_properties(muduo_protorpc_wire_cpp11 PROPERTIES COMPILE_FLAGS "--std=c++11 -Wno-error=shadow")
endif ()


if(NOT CMAKE_BUILD_NO_EXAMPLES)
add_executable(protobuf_rpc_wire_test RpcCodec_test.cc)
target_link_libraries(protobuf_rpc_wire_test muduo_protorpc_wire muduo_protobuf_codec)
if (NOT MSVC)
set_target_properties(protobuf_rpc_wire_test PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif ()
endif()

add_library(muduo_protorpc RpcChannel.cc RpcServer.cc)
if (MSVC)
target_link_libraries(muduo_protorpc muduo_protorpc_wire muduo_protobuf_codec muduo_net libprotobufd zlibstatic)
else(MSVC)
target_link_libraries(muduo_protorpc muduo_protorpc_wire muduo_protobuf_codec muduo_net protobuf z)
set_target_properties(muduo_protorpc PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()


if(TCMALLOC_LIBRARY)
  target_link_libraries(muduo_protorpc tcmalloc_and_profiler)
endif()

install(TARGETS muduo_protorpc_wire muduo_protorpc DESTINATION lib)
install(TARGETS muduo_protorpc_wire_cpp11 DESTINATION lib)

set(HEADERS
  RpcCodec.h
  RpcChannel.h
  RpcServer.h
  rpc.proto
  rpcservice.proto
  ${PROJECT_BINARY_DIR}/muduo/net/protorpc/rpc.pb.h
  )
install(FILES ${HEADERS} DESTINATION include/muduo/net/protorpc)

