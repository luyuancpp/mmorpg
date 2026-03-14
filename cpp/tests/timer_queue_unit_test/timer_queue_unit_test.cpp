#include <gtest/gtest.h>

#include "muduo/net/EventLoopThread.h"
#include <boost/date_time/posix_time/posix_time.hpp>

#include "time/comp/timer_task_comp.h"

#include <stdio.h>


using namespace muduo;
using namespace muduo::net;

int cnt = 0;
EventLoop* g_loop;

void printTid()
{
	//printf("pid = %d, tid = %d\n", getpid(), CurrentThread::tid());
	printf("now %s\n", Timestamp::now().toString().c_str());
}

void print(const char* msg)
{
	printf("msg %s %s\n", Timestamp::now().toString().c_str(), msg);
	if (++cnt == 20)
	{
		//g_loop->quit();
	}
}

void cancel(TimerId timer)
{
	g_loop->cancel(timer);
	printf("cancelled at %s\n", Timestamp::now().toString().c_str());
}


class GameTimerTest 
{
public:

	void RunAfter()
	{
		m_Timer.RunAfter(0.001, std::bind(&GameTimerTest::AfterCallBack, this));
		m_TimerActiveTest.RunAfter(1, std::bind(&GameTimerTest::AfterCallBackActiveTest, this));
	}


	void RunEvery()
	{
		m_Timer.RunEvery(0.001, std::bind(&GameTimerTest::RunEveryCallBack, this));
	}

    void RunAt(const Timestamp& time)
    {
        m_Timer.RunAt(time, std::bind(&GameTimerTest::RunAtCallBack, this));
    }

	void Cancel()
	{
		m_Timer.Cancel();
		assert(!m_Timer.IsActive());
	}

private:

	void AfterCallBack()
	{
		std::cout << "AfterCallBack" << this << std::endl;
	}

	void AfterCallBackActiveTest()
	{
		assert(!m_Timer.IsActive());
	}


	void RunEveryCallBack()
	{
		std::cout << "runEveryCallBack" << this  << std::endl;
	}

    void RunAtCallBack()
    {
        std::cout << "RunAtCallBack" << this << std::endl;
    }
private:
	TimerTaskComp m_Timer;
	TimerTaskComp m_TimerActiveTest;

};

class RecursionTimerTest
{
public:

    void RunAt(const Timestamp& time)
    {
        m_Timer.RunAt(time, std::bind(&RecursionTimerTest::RunAtCallBack, this));
    }

private:

    void RunAtCallBack()
    {
        std::cout << "RunAtCallBack" << this << std::endl;
        RunAt(Timestamp::fromUnixTime(time(NULL) + 5));
    }
private:
    TimerTaskComp m_Timer;
};

typedef std::shared_ptr<GameTimerTest> t_p;

typedef std::unordered_map<int32_t, t_p> t_list;

TEST(main, TimerQueueUnitTest)
{
	printTid();
	
	sleep(1);
	{
		EventLoop loop;
		g_loop = &loop;

		GameTimerTest a;
		a.RunAfter();

		GameTimerTest g;
		g.RunEvery();

		GameTimerTest c;
		c.RunEvery();
		c.Cancel();

		{
			GameTimerTest t;
			t.RunAfter();
		}

		{
			GameTimerTest t;
			t.RunEvery();
		}

		print("main");
		loop.runAfter(0.1, std::bind(print, "once0.1"));
		loop.runAfter(1, std::bind(print, "once1"));
		loop.runAfter(1.5, std::bind(print, "once1.5"));
		loop.runAfter(2.5, std::bind(print, "once2.5"));
		loop.runAfter(3.5, std::bind(print, "once3.5"));
		TimerId t45 = loop.runAfter(4.5, std::bind(print, "once4.5"));
		loop.runAfter(4.2, std::bind(cancel, t45));
		loop.runAfter(4.8, std::bind(cancel, t45));
		loop.runEvery(2, std::bind(print, "every2"));
		TimerId t3 = loop.runEvery(3, std::bind(print, "every3"));
		loop.runAfter(9.001, std::bind(cancel, t3));
        RecursionTimerTest r;
        r.RunAt(Timestamp::fromUnixTime(time(NULL) + 5));
        t_list v;
		while (true)
		{
            t_p p(new GameTimerTest);
            p->RunAt(Timestamp::fromUnixTime(time(NULL) + 5));
            v.emplace(1, p);
            v.erase(1);
            
			loop.loop();
		}
		
		print("main loop exits");
	}
	sleep(1);
	{
		EventLoopThread loopThread;
		EventLoop* loop = loopThread.startLoop();
		loop->runAfter(2, printTid);
		sleep(3);
		print("thread loop exits");
	}
}

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	int32_t nRet =  RUN_ALL_TESTS();
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	return nRet;
}