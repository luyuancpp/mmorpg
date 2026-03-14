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
#include <threading/registry_manager.h>

using namespace muduo;
using namespace muduo::net;


//对象1执行timer回调， 对象1设置对象2的timer的新的时间回调,
//对象2执行回调时候把自己的timer再重置(或者取消),
//对象2销毁,
//对象1设置的对象2的回调未取消

//队员1执行回调， 设置队员3的新的时间回调,
//队员3执行回调时候把自己的timer再重置(或者取消),
//队员3销毁,
//队员1设置的队员3的回调未取消，

int cnt = 0;
EventLoop* g_loop;


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
        std::cout << "runEveryCallBack" << this << std::endl;
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


class TestCoreDump {
public:

    TestCoreDump() { id = i++;  std::cout << "TestCoreCreate" << id << std::endl; }

    ~TestCoreDump() {
        std::cout << "TestCoreDump destroy ." << id << std::endl;
    }

    void TestTimer() {
        std::cout << "TestCoreDump : TestTimer." << &timer << std::endl;

        timer.RunAfter(0.1, [this]() {
            Add();
            });
    }

    void Add() {
        std::cout << "TestCoreDump : new callback executed." << &timer << std::endl;
        data_.push_back(1);
    }

    std::vector<int32_t> data_;

    TimerTaskComp timer;
    int32_t id{ 0 };

    static int32_t i;
};

int32_t TestCoreDump::i = 0;


struct CastingTimerCompTest
{
    TimerTaskComp timer;

    void Add() {
        std::cout << "TestCoreDump : new callback executed." << std::endl;
        data_.push_back(1);
    }

    std::vector<int32_t> data_;
};


void TestScenario() {

    auto entity = tlsRegistryManager.actorRegistry.create();

    auto& t = tlsRegistryManager.actorRegistry.emplace<CastingTimerCompTest>(entity);

    auto fn = [entity]() {
        std::cout << "TestCoreDump : new callback executed." << std::endl;

        tlsRegistryManager.actorRegistry.destroy(entity);

        auto entity = tlsRegistryManager.actorRegistry.create();
        auto& t = tlsRegistryManager.actorRegistry.get_or_emplace<CastingTimerCompTest>(entity);

        };

    t.timer.RunEvery(0.1, fn);


}


void TestScenario1() {

    using DestroyObjType = std::shared_ptr<TestCoreDump>;

    TimerTaskComp obj1;
    DestroyObjType obj2 = std::make_shared<DestroyObjType::element_type>();

    obj1.RunAfter(1.0, [&obj1, &obj2]() {
        std::cout << "Callback from obj2 executed." << obj2->id << std::endl;

        //对象1执行timer回调， 对象1设置对象2的timer的新的时间回调,
        obj2->timer.RunAfter(0.2, [&obj1, &obj2]() {
            std::cout << "obj2->timer.RunAfter" << &obj2->timer << std::endl;
            //对象2执行回调时候把自己的timer再重置(或者取消),
            obj2->TestTimer();
             
            });

        //对象2销毁,
        obj1.RunAfter(0.15, [&obj2]() {
            std::cout << "reset ." << &obj2->timer << std::endl;
            obj2.reset();
            });

        //对象1设置的对象2的回调未取消

        });


    g_loop->loop();

}

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
    EventLoop loop;
    g_loop = &loop;
    while (true)
    {
        TestScenario();

        {
            std::cout << " DungeonTimerTest " << std::endl;
            DungeonTimerTest t;
            t.Init();
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
        }

        {
            GameTimerTest t;
            t.RunAfter();
        }

        {
            GameTimerTest t;
            t.RunEvery();
        }

        {
            GameTimerTest t;
            t.RunEvery();
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

        TestScenario1();
    }

}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    int32_t nRet = RUN_ALL_TESTS();
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return nRet;
}