
if(MSVC)
LINK_DIRECTORIES("${protobuf_BINARY_DIR}/Debug")
LINK_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}/Debug")
endif (MSVC)
add_custom_command(OUTPUT resolver.pb.cc resolver.pb.h
  COMMAND protoc
  ARGS --cpp_out . ${CMAKE_CURRENT_SOURCE_DIR}/resolver.proto -I${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS resolver.proto)

if (NOT MSVC)
set_source_files_properties(resolver.pb.cc PROPERTIES COMPILE_FLAGS "-Wno-conversion -Wno-shadow")
endif()
include_directories(${PROJECT_BINARY_DIR})

add_library(resolver_proto resolver.pb.cc)
if (MSVC)
target_link_libraries(resolver_proto libprotobufd)
else(MSVC)
target_link_libraries(resolver_proto protobuf pthread)
endif()

add_executable(protobuf_rpc_resolver_client client.cc)
if (NOT MSVC)
set_target_properties(protobuf_rpc_resolver_client PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()
target_link_libraries(protobuf_rpc_resolver_client resolver_proto muduo_protorpc)

add_executable(protobuf_rpc_resolver_server server.cc)
if (NOT MSVC)
set_target_properties(protobuf_rpc_resolver_server PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()
if (MSVC)
target_link_libraries(protobuf_rpc_resolver_server resolver_proto muduo_protorpc)
else(MSVC)
target_link_libraries(protobuf_rpc_resolver_server resolver_proto muduo_protorpc muduo_cdns)
endif()
