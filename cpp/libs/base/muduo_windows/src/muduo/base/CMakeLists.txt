set(base_SRCS
  AsyncLogging.cc
  CountDownLatch.cc
  Date.cc
  Exception.cc
  FileUtil.cc
  LogFile.cc
  Logging.cc
  LogStream.cc
  ProcessInfo.cc
  Timestamp.cc
  TimeZone.cc
  ThreadPool.cc
  CurrentThread.cc
  CrossPlatformAdapterFunction.cc
  Thread.cc
  )
include_directories(${PROJECT_SOURCE_DIR}/contrib/windows/include)
add_library(muduo_base ${base_SRCS})

 target_link_libraries(muduo_base gtest)
if (UNIX)
 target_link_libraries(muduo_base pthread rt)
endif ()

add_library(muduo_base_cpp11 ${base_SRCS})
target_link_libraries(muduo_base_cpp11 gtest)

if (UNIX)
 target_link_libraries(muduo_base_cpp11 pthread rt)
 set_target_properties(muduo_base_cpp11 PROPERTIES COMPILE_FLAGS "--std=c++11")
endif ()

install(TARGETS muduo_base DESTINATION lib)
install(TARGETS muduo_base_cpp11 DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/muduo/base)

if(NOT CMAKE_BUILD_NO_EXAMPLES)
  add_subdirectory(tests)
endif()
