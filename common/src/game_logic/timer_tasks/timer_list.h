#pragma once

#include <unordered_map>

#include "GameTimer.h"

namespace muduo
{
    class TimerList
    {
    public:

        typedef std::shared_ptr<BaseModule::GameTimer> ptimer_type;

        typedef std::unordered_map<BaseModule::GameTimer *, ptimer_type> timelist_type;



        void Clear()
        {
            m_vTimerList.clear();
        }

        BaseModule::GameTimer * runAt(const Timestamp& time, const TimerCallback& cb)
        {
            ptimer_type  p( new BaseModule::GameTimer);
            p->runAt(time, cb);
            m_vTimerList.emplace(p.get(), p);
            return p.get();
        }

        BaseModule::GameTimer * runAfter(double delay, const TimerCallback& cb)
        {
            ptimer_type  p(new BaseModule::GameTimer);
            p->runAfter(delay, cb);
            m_vTimerList.emplace(p.get(), p);
            return p.get();
        }

        BaseModule::GameTimer * runEvery(double interval, const TimerCallback& cb)
        {
            ptimer_type  p(new BaseModule::GameTimer);
            p->runEvery(interval, cb);
            m_vTimerList.emplace(p.get(), p);
            return p.get();
        }

        void cancel(BaseModule::GameTimer *p)
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

