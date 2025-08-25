#pragma once
 #include <memory>
 #include <vector>

 #include "timer_duration.h"

 class DurationManager
 {
 public:
     using time_duration_ptr = std::unique_ptr<TimerDuration>;
     using timer_duration_vec = std::vector<time_duration_ptr>;

     bool InDuration(time_t t)
     {
         for (timer_duration_vec::iterator it = m_vTimerList.begin(); it != m_vTimerList.end(); ++it)
         {
             if ((*it)->InDuration(t))
             {
                 return true;
             }
         }
         return false;
     }

     bool IsOpen(time_t t)
     {
         return InDuration(t);
     }

     void Add(int32_t nType,
         time_t now,
         const std::string & sBeginTime,
         const std::string & sEndTime,
         const  muduo::net::TimerCallback & bCb,
         const  muduo::net::TimerCallback & eCb
     );

     void Add(int32_t nType,
         time_t now,
         time_t nBeginTime,
         time_t nEndTime,
         const  muduo::net::TimerCallback & bCb,
         const  muduo::net::TimerCallback & eCb
     );

     void SetCallBack(const  muduo::net::TimerCallback & bCb,
                         const  muduo::net::TimerCallback & eCb);
 private:
     timer_duration_vec m_vTimerList;
 };

