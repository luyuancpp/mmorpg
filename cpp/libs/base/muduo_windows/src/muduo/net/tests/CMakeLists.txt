add_executable(channeltest channeltest/ChannelTest.cc)
target_link_libraries(channeltest muduo_net)

add_executable(echoserverunittest echoserverunittest/EchoServerUnitTest.cc)
target_link_libraries(echoserverunittest muduo_net)

add_executable(echoclientunittest echoclientunittest/EchoClientUnitTest.cc)
target_link_libraries(echoclientunittest muduo_net)

add_executable(eventloopunittest eventloopunittest/EventLoopUnitTest.cc)
target_link_libraries(eventloopunittest muduo_net)

add_executable(eventloopthreadunittest eventloopthreadunittest/EventLoopThreadUnitTest.cc)
target_link_libraries(eventloopthreadunittest muduo_net)

add_executable(eventloopthreadpoolunittest eventloopthreadpoolunittest/EventLoopThreadPoolUnitTest.cc)
target_link_libraries(eventloopthreadpoolunittest muduo_net)

if(BOOSTTEST_LIBRARY)
add_executable(buffer_unittest Buffer_unittest.cc)
target_link_libraries(buffer_unittest muduo_net boost_unit_test_framework)
add_test(NAME buffer_unittest COMMAND buffer_unittest)

add_executable(buffer_cpp11_unittest Buffer_unittest.cc)
target_link_libraries(buffer_cpp11_unittest muduo_net_cpp11 boost_unit_test_framework)
if(UNIX)
 set_target_properties(buffer_cpp11_unittest PROPERTIES COMPILE_FLAGS "--std=c++20")
endif()

add_test(NAME buffer_cpp11_unittest COMMAND buffer_cpp11_unittest)

add_executable(inetaddress_unittest InetAddress_unittest.cc)
target_link_libraries(inetaddress_unittest muduo_net boost_unit_test_framework)
add_test(NAME inetaddress_unittest COMMAND inetaddress_unittest)

if(ZLIB_FOUND)
  add_executable(zlibstream_unittest ZlibStream_unittest.cc)
  target_link_libraries(zlibstream_unittest muduo_net boost_unit_test_framework z)
  # set_target_properties(zlibstream_unittest PROPERTIES COMPILE_FLAGS "--std=c++11")
endif()

endif()

add_executable(tcpclientreg1 tcpclientreg1/TcpClientReg1.cc)
target_link_libraries(tcpclientreg1 muduo_net)

add_executable(tcpclientreg2 tcpclientreg2/TcpClientReg2.cc)
target_link_libraries(tcpclientreg2 muduo_net)

add_executable(tcpclientreg3 tcpclientreg3/TcpClientReg3.cc)
target_link_libraries(tcpclientreg3 muduo_net)

add_executable(timerqueueunittest timerqueueunittest/TimerQueueUnitTest.cc)
target_link_libraries(timerqueueunittest muduo_net)
add_test(NAME timerqueueunittest COMMAND timerqueueunittest)

