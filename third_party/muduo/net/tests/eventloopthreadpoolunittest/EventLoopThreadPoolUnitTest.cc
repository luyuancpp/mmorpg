


#include <boost/bind.hpp>

#include <stdio.h>

#include <gtest/gtest.h>

#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/EventLoop.h>



using namespace muduo;
using namespace muduo::net;

void print(EventLoop* p = NULL)
{
	printf("main(): pid = %d, tid = %d, loop = %p\n",
		getpid(), CurrentThread::tid(), p);
}

void init(EventLoop* p)
{
	printf("init(): pid = %d, tid = %d, loop = %p\n",
		getpid(), CurrentThread::tid(), p);
}

TEST(EventLoopThreadPoolTest, ThreadPoolTest) 
{
	print();

	EventLoop loop;
	loop.runAfter(11, boost::bind(&EventLoop::quit, &loop));

	{
		printf("Single thread %p:\n", &loop);
		EventLoopThreadPool model(&loop, "single");
		model.setThreadNum(0);
		model.start(init);
		EXPECT_TRUE(model.getNextLoop() == &loop);
		EXPECT_TRUE(model.getNextLoop() == &loop);
		EXPECT_TRUE(model.getNextLoop() == &loop);
	}

	{
		printf("Another thread:\n");
		EventLoopThreadPool model(&loop, "another");
		model.setThreadNum(1);
		model.start(init);
		EventLoop* nextLoop = model.getNextLoop();
		nextLoop->runAfter(2, boost::bind(print, nextLoop));
		EXPECT_TRUE(nextLoop != &loop);
		EXPECT_TRUE(nextLoop == model.getNextLoop());
		EXPECT_TRUE(nextLoop == model.getNextLoop());
		::sleep(3);
	}

	{
		printf("Three threads:\n");
		EventLoopThreadPool model(&loop, "three");
		model.setThreadNum(3);
		model.start(init);
		EventLoop* nextLoop = model.getNextLoop();
		nextLoop->runInLoop(boost::bind(print, nextLoop));
		EXPECT_TRUE(nextLoop != &loop);
		EXPECT_TRUE(nextLoop != model.getNextLoop());
		EXPECT_TRUE(nextLoop != model.getNextLoop());
		EXPECT_TRUE(nextLoop == model.getNextLoop());
	}

	loop.loop();
}


int main(int argc, char **argv)
{

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

