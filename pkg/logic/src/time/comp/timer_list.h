#pragma once

#include <unordered_map>

#include "timer_task_comp.h"

namespace muduo
{
    class TimerList
    {
    public:

        using TimerPtr = std::shared_ptr<TimerTaskComp> ;

        using TimerListType =  std::unordered_map<TimerTaskComp *, TimerPtr>;

        void Clear()
        {
            timerList.clear();
        }

        TimerTaskComp * RunAt(const Timestamp& time, const TimerCallback& cb)
        {
            auto  p = std::make_unique<TimerTaskComp>();
            p->RunAt(time, cb);
            timerList.emplace(p.get(), p);
            return p.get();
        }

        TimerTaskComp * runAfter(double delay, const TimerCallback& cb)
        {
			auto  p = std::make_unique<TimerTaskComp>();
            p->RunAfter(delay, cb);
            timerList.emplace(p.get(), p);
            return p.get();
        }

        TimerTaskComp * RunEvery(double interval, const TimerCallback& cb)
        {
			auto  p = std::make_unique<TimerTaskComp>();
            p->RunEvery(interval, cb);
            timerList.emplace(p.get(), p);
            return p.get();
        }

        void Cancel(TimerTaskComp *p)
        {
            if (nullptr == p)
            {
                return;
            }
            timerList.erase(p);
        }


    private:
        TimerListType timerList;
    };
}

