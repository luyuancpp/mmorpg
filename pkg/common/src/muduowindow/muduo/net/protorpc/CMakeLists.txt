include_directories(${PROJECT_BINARY_DIR})

add_library(muduo_protorpc_wire RpcCodec.cc)
set_target_properties(muduo_protorpc_wire PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")

install(TARGETS muduo_protorpc_wire muduo_protorpc DESTINATION lib)
#install(TARGETS muduo_protorpc_wire_cpp11 DESTINATION lib)

set(HEADERS
  RpcCodec.h
  )
install(FILES ${HEADERS} DESTINATION include/muduo/net/protorpc)

