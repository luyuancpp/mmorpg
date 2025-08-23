// TcpClient::stop() called in the same iteration of IO event

#include <gtest/gtest.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>


using namespace muduo;
using namespace muduo::net;

TcpClient* g_client;

void timeout()
{
	LOG_INFO << "timeout";
	g_client->stop();
}

TEST(Main, tcpClientReg1)
{
	EventLoop loop;
	InetAddress serverAddr("127.0.0.1", 2); // no such server
	TcpClient client(&loop, serverAddr, "TcpClient");
	g_client = &client;
	loop.runAfter(0.0, timeout);
	loop.runAfter(1.0, std::bind(&EventLoop::quit, &loop));
	client.connect();
	CurrentThread::sleepUsec(100 * 1000);
	loop.loop();
}

int main(int argc, char **argv)
{

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
