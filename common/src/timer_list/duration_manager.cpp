 #include "DurationManager.h"

 void DurationManager::Add(int32_t nType,
     time_t now,
     const std::string & sBeginTime,
     const std::string & sEndTime,
     const muduo::TimerCallback & bCb,
     const muduo::TimerCallback & eCb
 )
 {
     time_duration_ptr ptr = TimerDuration::CreateDuration(nType, now, sBeginTime, sEndTime, bCb, eCb);
     if (ptr != NULL)
     {
         m_vTimerList.push_back(std::move(ptr));
     }
 }

 void DurationManager::Add(int32_t nType,
     time_t now,
     time_t nBeginTime,
     time_t nEndTime,
     const muduo::TimerCallback & bCb,
     const muduo::TimerCallback & eCb
 )
 {
     time_duration_ptr ptr = TimerDuration::CreateDuration(nType, now, nBeginTime, nEndTime, bCb, eCb);
     if (ptr != NULL)
     {
         m_vTimerList.push_back(std::move(ptr));
     }
 }

 void DurationManager::SetCallBack(const muduo::TimerCallback & bCb,
                                     const muduo::TimerCallback & eCb)
 {
     for (timer_duration_vec::iterator it = m_vTimerList.begin(); it != m_vTimerList.end(); ++it)
     {
         (*it)->SetCallBack(bCb, eCb);
     }
 }