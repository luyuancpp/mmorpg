#include <gtest/gtest.h>

#include <iostream>
#include <unordered_map>

#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include "time/comp/timer_task_comp.h"


#include <stdio.h>
#include <thread>
#include <chrono>

#ifdef __linux__
#include <unistd.h>
#endif // __linux__

using namespace muduo;
using namespace muduo::net;


//����ʱ�䵽�˻�ȡ��ʱ�䵽�˵Ķ��У���Ա1�ڶ�Ա3(timerΨһid3)ǰ�棬�ڶ�Ա1ִ�лص������У������˶�Ա3���µ�ʱ��ص���timer Ψһid6 ��,
// ���ǵ��˶�Ա3ִ�лص���ʱ������Լ����µ�id ���ã�timer Ψһid��Ч ����
// ����֮�������Ա3���������ڱ�timerΨһid6���������ڶ̣���Ա3�����ˣ�
// ���Ǳ���Ա1����timer�� Ψһid6)δȡ������Ϊ������Ա3ʱ�䵽��ʱ���û�����Ч�ˣ�������ȥ���ö�Ա3���Ұ����

//��Ա1ִ�лص��� ���ö�Ա3���µ�ʱ��ص�,
//��Ա3ִ�лص�ʱ����Լ���timer������(����ȡ��),
//��Ա3����,
//��Ա1���õĶ�Ա3�Ļص�δȡ����

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
		m_Timer.RunAfter(0.1, std::bind(&GameTimerTest::AfterCallBack, this));
	}

	void RunEvery()
	{
		m_Timer.RunEvery(0.1, std::bind(&GameTimerTest::RunEveryCallBack, this));
	}

    void RunAt(const Timestamp& time)
    {
        m_Timer.RunAt(time, std::bind(&GameTimerTest::RunAtCallBack, this));
    }



private:

	void AfterCallBack()
	{
		std::cout << "AfterCallBack" << this << std::endl;
        RunAfter();
	}

	void RunEveryCallBack()
	{
		std::cout << "runEveryCallBack" << this  << std::endl;
        RunEvery();
	}

    void RunAtCallBack()
    {
        std::cout << "RunAtCallBack" << this << std::endl;
        
    }
private:
	TimerTaskComp m_Timer;
    TimerTaskComp m_Timer1;
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


class DungeonTimerTest
{
public:

    void Init()
    {
        m_PrepareEndTimer.RunAfter(0.01, std::bind(&DungeonTimerTest::PrePare, this));
        m_ConclusionTimer.RunAfter(0.01, std::bind(&DungeonTimerTest::DungeonFinish, this));
        m_DurationTimer.RunAfter(0.01, std::bind(&DungeonTimerTest::ForceRetreatScene, this));

    }

    void PrePare()
    {
        std::chrono::seconds s2(1);
        std::this_thread::sleep_for(s2);
        m_ConclusionTimer.RunAfter(0.01, std::bind(&DungeonTimerTest::DungeonFinish, this));
    }

    void DungeonFinish()
    {
        std::chrono::seconds s2(2);
        std::this_thread::sleep_for(s2);
        m_DurationTimer.RunAfter(0.01, std::bind(&DungeonTimerTest::ForceRetreatScene, this));
    }

    void ForceRetreatScene()
    {
        std::chrono::seconds s2(1);
        std::this_thread::sleep_for(s2);
        m_PrepareEndTimer.RunAfter(0.01, std::bind(&DungeonTimerTest::PrePare, this));

    }

    TimerTaskComp m_PrepareEndTimer;
    TimerTaskComp m_ConclusionTimer;
    TimerTaskComp m_DurationTimer;
   
};

TEST(main, TimerQueueUnitTest)
{
	printTid();
    EventLoop loop;
    g_loop = &loop;
    while (true)
    {
        {
            std::cout << " DungeonTimerTest " << std::endl;
            DungeonTimerTest t;
            t.Init();
            loop.loop();
        }
        
        GameTimerTest a1;
        loop.loop();

		GameTimerTest a;
		a.RunAfter();

		GameTimerTest g;
		g.RunEvery();

		GameTimerTest c;
		c.RunEvery();



		{
			GameTimerTest t;
			t.RunAfter();
            loop.loop();
		}

        {
            GameTimerTest t;
            t.RunAfter();
            loop.loop();
        }

		{
			GameTimerTest t;
			t.RunEvery();
            loop.loop();
		}

        {
            GameTimerTest t;
            t.RunEvery();
            loop.loop();
        }



        RecursionTimerTest r;
        r.RunAt(Timestamp::fromUnixTime(time(NULL) + 5));
        t_list v;
	
        {
            t_p p(new GameTimerTest);
            p->RunAt(Timestamp::fromUnixTime(time(NULL) + 5));
        }
        std::chrono::seconds s2(10);
        std::this_thread::sleep_for(s2);
        loop.loop();
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