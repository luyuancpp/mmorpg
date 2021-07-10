 #ifndef __DURATION_MANAGER_H
 #define __DURATION_MANAGER_H
 #include <memory>
 #include <vector>

 #include "CommonLogic/TimerList/TimerDuration.h"

 class DurationManager
 {
 public:
     typedef std::unique_ptr<TimerDuration> time_duration_ptr;
     typedef std::vector<time_duration_ptr> timer_duration_vec;

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
         const muduo::TimerCallback & bCb,
         const muduo::TimerCallback & eCb
     );

     void Add(int32_t nType,
         time_t now,
         time_t nBeginTime,
         time_t nEndTime,
         const muduo::TimerCallback & bCb,
         const muduo::TimerCallback & eCb
     );

     void SetCallBack(const muduo::TimerCallback & bCb,
                         const muduo::TimerCallback & eCb);
 private:
     timer_duration_vec m_vTimerList;
 };

 #endif // __DURATION_MANAGER_H
