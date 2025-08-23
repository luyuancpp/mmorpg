#pragma once

#include "muduo/net/Callbacks.h"
#include "muduo/net/EventLoop.h"

using muduo::net::TimerId;
using muduo::Timestamp;
using muduo::net::TimerCallback;
using muduo::net::EventLoop;

//A类的定时器不能放到B类里面，否者B类定时(调用A类函数)器走的时候A已经销毁
class TimerTaskComp
{
public:
    TimerTaskComp();
	~TimerTaskComp();

    TimerTaskComp(const TimerTaskComp&);
    TimerTaskComp& operator=(const TimerTaskComp&) = delete;

	TimerTaskComp(TimerTaskComp&& param) noexcept;
    TimerTaskComp& operator=(TimerTaskComp&& param) noexcept;

	void RunAt(const Timestamp& time, const TimerCallback& cb);
	void RunAfter(double delay, const TimerCallback& cb);
	void RunEvery(double interval, const TimerCallback& cb);
    void Run() const;

	void Cancel();

    bool IsActive();

    uint64_t GetEndTime();

    void SetCallBack(const TimerCallback& cb);
private:
    void OnTimer();

    TimerId  timerId;
    TimerCallback callback;
};





