#pragma once

#include "muduo/net/Callbacks.h"
#include "muduo/net/EventLoop.h"

using muduo::net::TimerId;
using muduo::Timestamp;
using muduo::net::TimerCallback;
using muduo::net::EventLoop;

// Timer must be owned by its callback target; if B owns A's timer,
// A may be destroyed before B's timer fires.
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





