add_executable(asyncloggingtest asyncloggingtest/AsyncLoggingTest.cc)
target_link_libraries(asyncloggingtest muduo_base)

add_executable(blockingqueuetest blockingqueuetest/BlockingQueueTest.cc)
target_link_libraries(blockingqueuetest muduo_base)

add_executable(blockingqueue_cpp11_test blockingqueuetest/BlockingQueueTest.cc)
target_link_libraries(blockingqueue_cpp11_test muduo_base_cpp11)
if(UNIX)
set_target_properties(blockingqueue_cpp11_test PROPERTIES COMPILE_FLAGS "--std=c++11")
endif()

add_executable(boundedblockingqueuetest boundedblockingqueuetest/BoundedBlockingQueueTest.cc)
target_link_libraries(boundedblockingqueuetest muduo_base)

add_executable(datetest datetest/DateTest.cc)
target_link_libraries(datetest muduo_base)
add_test(NAME datetest COMMAND datetest)

add_executable(exceptiontest exceptiontest/ExceptionTest.cc)
target_link_libraries(exceptiontest muduo_base)
add_test(NAME exceptiontest COMMAND exceptiontest)

add_executable(fileutiltest fileutiltest/FileUtilTest.cc)
target_link_libraries(fileutiltest muduo_base)
add_test(NAME fileutiltest COMMAND fileutiltest)


if(ZLIB_FOUND)
  add_executable(gzipfile_test GzipFile_test.cc)
  target_link_libraries(gzipfile_test muduo_base_cpp11 z)
  set_target_properties(gzipfile_test PROPERTIES COMPILE_FLAGS "--std=c++11")
  add_test(NAME gzipfile_test COMMAND gzipfile_test)
endif()

add_executable(logfiletest logfiletest/LogFileTest.cc)
target_link_libraries(logfiletest muduo_base)

add_executable(logstreambench logstreambench/LogStreamBench.cc)
target_link_libraries(logstreambench muduo_base)

if(BOOSTTEST_LIBRARY)
add_executable(logstream_test LogStream_test.cc)
target_link_libraries(logstream_test muduo_base boost_unit_test_framework)
add_test(NAME logstream_test COMMAND logstream_test)
endif()

add_executable(threadbench threadbench/ThreadBench.cc)
target_link_libraries(threadbench muduo_base)

add_executable(threadtest threadtest/ThreadTest.cc)
target_link_libraries(threadtest muduo_base)

add_executable(threadpooltest threadpooltest/ThreadPoolTest.cc)
target_link_libraries(threadpooltest muduo_base)

add_executable(timestamptest timestamptest/TimestampTest.cc)
target_link_libraries(timestamptest muduo_base)
add_test(NAME timestamptest COMMAND timestamptest)

add_executable(timezonetest timezonetest/TimeZoneTest.cc)
target_link_libraries(timezonetest muduo_base)
add_test(NAME timezonetest COMMAND timezonetest)

