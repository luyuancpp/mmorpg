if(MSVC)
LINK_DIRECTORIES("${protobuf_BINARY_DIR}/Debug")
LINK_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}/Debug")
endif (MSVC)

add_custom_command(OUTPUT echo.pb.cc echo.pb.h
  COMMAND protoc
  ARGS --cpp_out . ${CMAKE_CURRENT_SOURCE_DIR}/echo.proto -I${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS echo.proto)

if (NOT MSVC)
set_source_files_properties(echo.pb.cc PROPERTIES COMPILE_FLAGS "-Wno-conversion -Wno-shadow")
endif()
include_directories(${PROJECT_BINARY_DIR})

add_library(echo_proto echo.pb.cc)
if (MSVC)
target_link_libraries(echo_proto libprotobufd)
else(MSVC)
target_link_libraries(echo_proto protobuf pthread)
endif()

add_executable(protobuf_rpc_echo_client client.cc)
if (NOT MSVC)
set_target_properties(protobuf_rpc_echo_client PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()
target_link_libraries(protobuf_rpc_echo_client echo_proto muduo_protorpc)

add_executable(protobuf_rpc_echo_server server.cc)
if (NOT MSVC)
set_target_properties(protobuf_rpc_echo_server PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif()
target_link_libraries(protobuf_rpc_echo_server echo_proto muduo_protorpc)
