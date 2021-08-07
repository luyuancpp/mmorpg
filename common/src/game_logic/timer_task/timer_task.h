#ifndef COMMON_SRC_timer_task
#define COMMON_SRC_timer_task

#include	"muduo/net/EventLoop.h"

namespace common
{
    const int32_t kDaysPerWeek = 7;
    const int32_t kWeekBeginDay = 1;
    const int32_t kSecondsPerDay = 86400;
    const int32_t kSecondsPerWeek = 24 * 60 * 60 * 7;
    const int32_t kSecondsPerHours = 60 * 60;
    const int32_t kSecondsPerMinute = 60;
    const int32_t kMillisecondsPerSecond = 1000;

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
        void UpdateEndTeamDate();

        void SetCallBack(const TimerCallback& cb);
	private:
        void OnTimer();
	private:
        TimerId  timer_id_;
        TimerCallback timer_function_callback_;
        Timestamp end_timestamp_;
	};

} // namespace BaseModule

#endif // COMMON_SRC_timer_task

