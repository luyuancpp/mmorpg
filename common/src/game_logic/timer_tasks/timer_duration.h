 #ifndef __DURATION_TIMER_
 #define __DURATION_TIMER_
 #include <ctime>

 #include <cstdint>

#include "GameTimer.h"

 static muduo::TimerCallback kEmptyCallBack;


 class TimerDuration
 {
 public:
     typedef std::unique_ptr<TimerDuration> time_duration_ptr;

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

     TimerDuration(time_t nBeginTime, time_t nEndTime, const muduo::TimerCallback & bCb, const muduo::TimerCallback & eCb);
     TimerDuration(const std::string & sBeginTime, const std::string & sEndTime, const muduo::TimerCallback & bCb, const muduo::TimerCallback & eCb);

     void CalcBeginEndTime();
     void CalcBeginEndTime(time_t tNow);

     bool InDuration(time_t t)
     {
         return m_nBeginTime <= t && t <= m_nEndTime;
     }

     bool IsOpen(time_t t)
     {
         return InDuration(t);
     }

     time_t GetRemainTime(time_t t)
     {
         if (m_nEndTime < t)
         {
             return 0;
         }
         return m_nEndTime - t;
     }

     void SetBeginTime(time_t nBeginTime)
     {
         m_nBeginTime = nBeginTime;
     }

     time_t GetEndTime()
     {
         return m_nEndTime;
     }

     time_t GetBeginTime()
     {
         return m_nBeginTime;
     }

     void OnBegin()
     {
         if (m_bBeginCallBack)
         {
             m_bBeginCallBack();
         }
     }

     void OnEnd()
     {
         if (m_bEndCallBack)
         {
             m_bEndCallBack();
         }
         CalcNextDuration();
     }

     void SetCallBack(const muduo::TimerCallback & bCb,
         const muduo::TimerCallback & eCb);

     virtual void CalcNextDuration()
     {

     }

	 void InitTimer();

     static time_duration_ptr CreateDuration(int32_t nType,
         time_t now,
                                             const std::string & sBeginTime,
                                             const std::string & sEndTime,
                                             const muduo::TimerCallback & bCb,
                                             const muduo::TimerCallback & eCb
                                             );

     static time_duration_ptr CreateDuration(int32_t nType,
         time_t now,
         time_t nBeginTime,
         time_t nEndTime,
         const muduo::TimerCallback & bCb,
         const muduo::TimerCallback & eCb
     );

 protected:
     time_t m_nBeginTime{0};
     time_t m_nEndTime{ 0 };
     BaseModule::GameTimer m_oBeginTimer;
     BaseModule::GameTimer m_oEndTimer;
     muduo::TimerCallback m_bBeginCallBack;
     muduo::TimerCallback m_bEndCallBack;
 };






 class DailyTimerDuration : public TimerDuration
 {
 public:
     using TimerDuration::TimerDuration;



     virtual void CalcNextDuration()
     {
         m_nBeginTime += kSecondsPerDay ;
         m_nEndTime += kSecondsPerDay;
         InitTimer();
     }
 };


 class WeekTimerDuration : public TimerDuration
 {
 public:
     using TimerDuration::TimerDuration;



     virtual void CalcNextDuration()
     {
         m_nBeginTime += kSecondsPerWeek;
         m_nEndTime += kSecondsPerWeek;
		 InitTimer();
     }
 };


 #endif // !__DURATION_TIMER_
