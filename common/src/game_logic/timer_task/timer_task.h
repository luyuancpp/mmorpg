#pragma once

#include	"muduo/net/EventLoop.h"

using muduo::net::TimerId;
using muduo::Timestamp;
using muduo::net::TimerCallback;
using muduo::net::EventLoop;

    //A类的定时器不能放到B类里面，否者B类定时(调用A类函数)器走的时候A已经销毁
class TimerTask
{
public:
    TimerTask() {}
	~TimerTask();

    TimerTask(const TimerTask &) = delete;
    TimerTask & operator=(const TimerTask &) = delete;

	void RunAt(const Timestamp& time, const TimerCallback& cb);
	void RunAfter(double delay, const TimerCallback& cb);
	void RunEvery(double interval, const TimerCallback& cb);
    void Call();

	void Cancel();

    bool ActiveTimer();

    int32_t GetEndTime();
    void UpdateEndStamp();

    void SetCallBack(const TimerCallback& cb);
private:
    void OnTimer();
private:
    TimerId  timer_id_;
    TimerCallback timer_function_callback_;
    Timestamp end_timestamp_;
};





