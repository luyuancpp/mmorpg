// TcpClient destructs in a different thread.

#include <gtest/gtest.h>

#include <muduo/base/CurrentThread.h>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/TcpClient.h>


#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

TEST( Main, tcpClientReg3)
{
	Logger::setLogLevel(Logger::DEBUG);

	EventLoopThread loopThread;
	{
		InetAddress serverAddr("127.0.0.1", 1234); // should succeed
		TcpClient client(loopThread.startLoop(), serverAddr, "TcpClient");
		client.connect();
		CurrentThread::sleepUsec(500 * 1000);  // wait for connect
		client.disconnect();
	}

	CurrentThread::sleepUsec(1000 * 1000);
}

int main(int argc, char **argv)
{

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
