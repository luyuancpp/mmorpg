#pragma once
 #include <ctime>

 #include <cstdint>

#include "timer_task.h"
#include "constants/time.h"

 static muduo::net::TimerCallback kEmptyCallBack;


 class TimerDuration
 {
 public:
     using time_duration_ptr = std::unique_ptr<TimerDuration> ;

     enum eDurationEnum
     {
         E_NORMAL, //指定一个固定时间区域比如:2018:3:12 0:0:0 - 2018:3:13 0:0:0
         E_DURATION_DAILY,//指定一个固定时间区域比如: 0:0:0 -  0:0:0
         E_DURATION_WEEK,//指定一个固定时间区域比如: 1 0:0:0 -  1 0:0:0
         E_DURATION_MONTH,
         E_DURATION_MAX,
     };

     enum eDurationStatus
     {
         E_UN_OPEN,
         E_OPEN,
         E_OVER,
     };

     TimerDuration(time_t nBeginTime, time_t nEndTime);

     TimerDuration(time_t nBeginTime, time_t nEndTime, 
         const muduo::net::TimerCallback & bCb, const muduo::net::TimerCallback & eCb);
     TimerDuration(const std::string & sBeginTime, 
         const std::string & sEndTime, 
         const  muduo::net::TimerCallback & bCb, const  muduo::net::TimerCallback & eCb);

     void CalcBeginEndTime();
     void CalcBeginEndTime(time_t tNow);

     bool InDuration(time_t t)
     {
         return begin_time_ <= t && t <= end_time_;
     }

     bool IsOpen(time_t t)
     {
         return InDuration(t);
     }

     time_t GetRemainTime(time_t t)
     {
         if (end_time_ < t)
         {
             return 0;
         }
         return end_time_ - t;
     }

     void SetBeginTime(time_t nBeginTime)
     {
         begin_time_ = nBeginTime;
     }

     time_t GetEndTime()
     {
         return end_time_;
     }

     time_t GetBeginTime()
     {
         return begin_time_;
     }

     void OnBegin()
     {
         if (begin_callback_)
         {
             begin_callback_();
         }
     }

     void OnEnd()
     {
         if (end_callback_)
         {
             end_callback_();
         }
         CalcNextDuration();
     }

     void SetCallBack(const muduo::net::TimerCallback & bCb,
         const  muduo::net::TimerCallback & eCb);

     virtual void CalcNextDuration()
     {

     }

	 void InitTimer();

     static time_duration_ptr CreateDuration(int32_t nType,
         time_t now,
                                             const std::string & sBeginTime,
                                             const std::string & sEndTime,
                                             const  muduo::net::TimerCallback & bCb,
                                             const  muduo::net::TimerCallback & eCb
                                             );

     static time_duration_ptr CreateDuration(int32_t nType,
         time_t now,
         time_t nBeginTime,
         time_t nEndTime,
         const  muduo::net::TimerCallback & bCb,
         const  muduo::net::TimerCallback & eCb
     );

 protected:
     time_t begin_time_{0};
     time_t end_time_{ 0 };
     TimerTask begin_timer_;
     TimerTask end_timer_;
      muduo::net::TimerCallback begin_callback_;
      muduo::net::TimerCallback end_callback_;
 };






 class DailyTimerDuration : public TimerDuration
 {
 public:
     using TimerDuration::TimerDuration;



     virtual void CalcNextDuration()
     {
         begin_time_ += kSecondsPerDay ;
         end_time_ += kSecondsPerDay;
         InitTimer();
     }
 };


 class WeekTimerDuration : public TimerDuration
 {
 public:
     using TimerDuration::TimerDuration;



     virtual void CalcNextDuration()
     {
         begin_time_ += kSecondsPerWeek;
         end_time_ += kSecondsPerWeek;
		 InitTimer();
     }
 };


