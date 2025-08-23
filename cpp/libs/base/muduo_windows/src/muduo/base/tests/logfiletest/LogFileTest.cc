
#include <thread>

#include <gtest/gtest.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/Logging.h>

using namespace muduo;

std::unique_ptr<muduo::LogFile> g_logFile;


void outputFunc(const char* msg, int len)
{
	g_logFile->append(msg, len);
}

void flushFunc()
{
	g_logFile->flush();
}


TEST(LogFileTest, MultipleThread)
{
	g_logFile.reset(new muduo::LogFile("test", 200 * 1000));
	muduo::Logger::setOutput(outputFunc);
	muduo::Logger::setFlush(flushFunc);

	std::string line = "1234567890 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

	for (int i = 0; i < 10000; ++i)
	{
		LOG_INFO << line << i;

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

}

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

