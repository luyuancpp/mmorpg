
if(MSVC)
LINK_DIRECTORIES("${protobuf_BINARY_DIR}/Debug")
LINK_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}/Debug")
endif (MSVC)

add_library(muduo_protobuf_codec ProtobufCodecLite.cc)
if (NOT MSVC)
set_target_properties(muduo_protobuf_codec PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
endif ()
if (MSVC)
target_link_libraries(muduo_protobuf_codec muduo_net libprotobufd zlibstatic)
else(MSVC)
target_link_libraries(muduo_protobuf_codec muduo_net protobuf z)
endif ()

add_library(muduo_protobuf_codec_cpp11 ProtobufCodecLite.cc)


if (UNIX)
 set_target_properties(muduo_protobuf_codec_cpp11 PROPERTIES COMPILE_FLAGS "--std=c++11 -Wno-error=shadow")
endif ()

if (MSVC)
else(MSVC)
target_link_libraries(muduo_protobuf_codec_cpp11 muduo_net_cpp11 protobuf z)
endif()


install(TARGETS muduo_protobuf_codec DESTINATION lib)
install(TARGETS muduo_protobuf_codec_cpp11 DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/muduo/net/protobuf)

