#pragma once

#include "muduo/net/EventLoop.h"
#include "muduo/net/Callbacks.h"

using muduo::net::TimerId;
using muduo::Timestamp;
using muduo::net::TimerCallback;
using muduo::net::EventLoop;

//A类的定时器不能放到B类里面，否者B类定时(调用A类函数)器走的时候A已经销毁
class TimerTaskComp
{
public:
    TimerTaskComp() {}
	~TimerTaskComp();

    TimerTaskComp(const TimerTaskComp &) = delete;
    TimerTaskComp & operator=(const TimerTaskComp &) = delete;

	void RunAt(const Timestamp& time, const TimerCallback& cb);
	void RunAfter(double delay, const TimerCallback& cb);
	void RunEvery(double interval, const TimerCallback& cb);
    void Call();

	void Cancel();

    bool Actived();

    int32_t GetEndTime();
    void UpdateEndStamp();

    void SetCallBack(const TimerCallback& cb);
private:
    void OnTimer();

    TimerId  id_;
    TimerCallback callback_;
    Timestamp end_time_;
};





