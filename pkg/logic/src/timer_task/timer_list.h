#pragma once

#include <unordered_map>

#include "timer_task/timer_task.h"

namespace muduo
{
    class TimerList
    {
    public:

        using ptimer_type = std::shared_ptr<TimerTask> ;

        using timelist_type =  std::unordered_map<TimerTask *, ptimer_type>;

        void Clear()
        {
            m_vTimerList.clear();
        }

        TimerTask * RunAt(const Timestamp& time, const TimerCallback& cb)
        {
            ptimer_type  p( new TimerTask);
            p->RunAt(time, cb);
            m_vTimerList.emplace(p.get(), p);
            return p.get();
        }

        TimerTask * runAfter(double delay, const TimerCallback& cb)
        {
            ptimer_type  p(new TimerTask);
            p->RunAfter(delay, cb);
            m_vTimerList.emplace(p.get(), p);
            return p.get();
        }

        TimerTask * RunEvery(double interval, const TimerCallback& cb)
        {
            ptimer_type  p(new TimerTask);
            p->RunEvery(interval, cb);
            m_vTimerList.emplace(p.get(), p);
            return p.get();
        }

        void Cancel(TimerTask *p)
        {
            if (nullptr == p)
            {
                return;
            }
            m_vTimerList.erase(p);
        }


    private:
        timelist_type m_vTimerList;
    };
}

