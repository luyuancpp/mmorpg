#include <atomic>
#include <functional>
#include <mutex>

#include <gtest/gtest.h>


#include <muduo/base/CurrentThread.h>
#include <muduo/base/Timestamp.h>

std::mutex g_mutex;
std::map<int, int> g_delays;
std::atomic<int> numCreated;

void threadFunc()
{
	//printf("tid=%d\n", muduo::CurrentThread::tid());
	++numCreated;
}

void threadFunc2(muduo::Timestamp start)
{
	muduo::Timestamp now(muduo::Timestamp::now());
	int delay = static_cast<int>(timeDifference(now, start) * 1000000);
	std::unique_lock<std::mutex> lock(g_mutex);
	++g_delays[delay];
}



TEST(TestThreadBench, TestThreadBench)
{
	printf("tid=%d\n",  muduo::CurrentThread::tid());
	muduo::Timestamp start(muduo::Timestamp::now());

	int kThreads = 100 * 1000;
	for (int i = 0; i < kThreads; ++i)
	{
		std::thread t1(threadFunc);
		t1.join();
	}

	double timeUsed = timeDifference(muduo::Timestamp::now(), start);
	printf("thread creation time %f us\n", timeUsed * 1000000 / kThreads);
	printf("number of created threads %d\n", numCreated.load(std::memory_order_relaxed));

	for (int i = 0; i < kThreads; ++i)
	{
		muduo::Timestamp now(muduo::Timestamp::now());
		std::thread t2(std::bind(threadFunc2, now));
		
		t2.join();
	}
	{
		std::unique_lock<std::mutex> lock(g_mutex);
		for (std::map<int, int>::iterator it = g_delays.begin();
			it != g_delays.end(); ++it)
		{
			printf("delay = %d, count = %d\n",
				it->first, it->second);
		}
	}


}


int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}


