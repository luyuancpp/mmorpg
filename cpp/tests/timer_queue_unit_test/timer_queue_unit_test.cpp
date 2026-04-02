#include <gtest/gtest.h>

#include "muduo/net/EventLoopThread.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <utility>

#include "time/comp/timer_task_comp.h"

using namespace muduo;
using namespace muduo::net;

class GameTimerTest
{
public:
	GameTimerTest(std::atomic<int>& afterCount, std::atomic<int>& everyCount)
		: afterCount_(afterCount), everyCount_(everyCount) {}

	void RunAfter()
	{
		m_Timer.RunAfter(0.01, std::bind(&GameTimerTest::AfterCallBack, this));
		m_TimerActiveTest.RunAfter(0.05, std::bind(&GameTimerTest::AfterCallBackActiveTest, this));
	}


	void RunEvery()
	{
		m_Timer.RunEvery(0.01, std::bind(&GameTimerTest::RunEveryCallBack, this));
	}

	void Cancel()
	{
		m_Timer.Cancel();
		assert(!m_Timer.IsActive());
	}

private:

	void AfterCallBack()
	{
		++afterCount_;
	}

	void AfterCallBackActiveTest()
	{
		assert(!m_Timer.IsActive());
	}


	void RunEveryCallBack()
	{
		++everyCount_;
    }
private:
	std::atomic<int>& afterCount_;
	std::atomic<int>& everyCount_;
	TimerTaskComp m_Timer;
	TimerTaskComp m_TimerActiveTest;

};

TEST(TimerQueueTest, BasicTimerOperations)
{
	std::atomic<int> afterCount{0};
	std::atomic<int> everyCount{0};

	EventLoop loop;
	GameTimerTest oneShot(afterCount, everyCount);
	oneShot.RunAfter();

	GameTimerTest periodic(afterCount, everyCount);
	periodic.RunEvery();

	loop.runAfter(0.08, [&periodic]() {
		periodic.Cancel();
	});

	loop.runAfter(0.12, [&loop]() {
		loop.quit();
	});

	loop.loop();

	EXPECT_GE(afterCount.load(), 1);
	EXPECT_GE(everyCount.load(), 1);
}

TEST(TimerQueueTest, SafeWithoutEventLoop)
{
	TimerTaskComp timer;

	timer.RunAfter(0.01, []() {});
	timer.RunEvery(0.01, []() {});
	timer.RunAt(Timestamp::now(), []() {});
	timer.Cancel();

	EXPECT_FALSE(timer.IsActive());
	EXPECT_EQ(timer.GetEndTime(), 0U);
}

TEST(TimerQueueTest, MoveAndCopyAreSafe)
{
	TimerTaskComp source;
	source.SetCallBack([]() {});

	TimerTaskComp copied(source);
	EXPECT_FALSE(copied.IsActive());
	EXPECT_EQ(copied.GetEndTime(), 0U);

	TimerTaskComp moved(std::move(source));
	EXPECT_FALSE(moved.IsActive());
	EXPECT_EQ(moved.GetEndTime(), 0U);
}

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}