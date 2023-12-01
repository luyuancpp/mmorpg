// TcpClient destructs when TcpConnection is connected but unique.


#include <gtest/gtest.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>


using namespace muduo;
using namespace muduo::net;

void threadFunc(EventLoop* loop)
{
	InetAddress serverAddr("127.0.0.1", 1234); // should succeed
	TcpClient client(loop, serverAddr, "TcpClient");
	client.connect();

	CurrentThread::sleepUsec(1000 * 1000);
	// client destructs when connected.
}

TEST(main, tcpClientReg2)
{
	Logger::setLogLevel(Logger::DEBUG);

	EventLoop loop;
	loop.runAfter(3.0, std::bind(&EventLoop::quit, &loop));
	std::thread thr(std::bind(threadFunc, &loop));
	
	loop.loop();
    thr.join();
}


int main(int argc, char **argv)
{

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
