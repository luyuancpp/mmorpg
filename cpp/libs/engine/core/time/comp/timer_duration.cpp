#include "timer_duration.h"

#include "duration_funtion.h"
#include "time/system/time.h"

TimerDuration::TimerDuration(uint64_t nBeginTime, uint64_t nEndTime)
    : TimerDuration(nBeginTime, nEndTime, kEmptyCallBack, kEmptyCallBack)
{
}

TimerDuration::TimerDuration(uint64_t nBeginTime, uint64_t nEndTime, 
    const  muduo::net::TimerCallback & bCb, 
    const  muduo::net::TimerCallback & eCb)
    : begin_time_(nBeginTime),
    end_time_(nEndTime),
    begin_callback_(bCb),
    end_callback_(eCb)
{
}

TimerDuration::TimerDuration(const std::string & sBeginTime, const std::string & sEndTime, 
    const  muduo::net::TimerCallback & bCb, 
    const  muduo::net::TimerCallback & eCb)
    : TimerDuration(YmdHmsStringToTime(sBeginTime), YmdHmsStringToTime(sEndTime), bCb, eCb)
{
}

void TimerDuration::CalcBeginEndTime()
{
    CalcBeginEndTime(TimeSystem::NowSecondsUTC());
}

void TimerDuration::CalcBeginEndTime(uint64_t tNow)
{
    if (tNow <= end_time_)
    {
		InitTimer();
    }
    else
    {
        CalcNextDuration();
    }
}

void TimerDuration::SetCallBack(const  muduo::net::TimerCallback & bCb,
    const  muduo::net::TimerCallback & eCb)
{
    begin_callback_ = bCb;
    end_callback_ = eCb;
}

void TimerDuration::InitTimer()
{
	begin_timer_.RunAt(muduo::Timestamp::fromUnixTime(begin_time_), std::bind(&TimerDuration::OnBegin, this));
	end_timer_.RunAt(muduo::Timestamp::fromUnixTime(end_time_), std::bind(&TimerDuration::OnEnd, this));
}

TimerDuration::TimerDurationPtr TimerDuration::CreateDuration(int32_t nType,
    uint64_t now,
    const std::string & sBeginTime,
    const std::string & sEndTime,
    const  muduo::net::TimerCallback & bCb,
    const  muduo::net::TimerCallback & eCb)
{
    TimerDurationPtr ptr;
    switch (nType)
    {

    case E_NORMAL:
    {
        return  CreateDuration(nType, now, sBeginTime, sEndTime, bCb, eCb);
    }
    break;
    case E_DURATION_DAILY:
    {
        muduo::Date dateToday(GetTodayDate(now));
        return  CreateDuration(nType, now, GetTime(dateToday, sBeginTime), GetTime(dateToday, sEndTime), bCb, eCb);
    }
    break;
    case  E_DURATION_WEEK:
    {
        muduo::Date dateToday(GetTodayDate(now));
        muduo::Date dateWeekBegin(GetWeekBeginDay(dateToday));
        return  CreateDuration(nType, now, GetWeekTime(dateWeekBegin, sBeginTime), GetWeekTime(dateWeekBegin, sEndTime), bCb, eCb);
    }
    break;
    default:
        break;
    }
    return ptr;
}

TimerDuration::TimerDurationPtr TimerDuration::CreateDuration(int32_t nType,
    uint64_t now,
    uint64_t nBeginTime,
    uint64_t nEndTime,
    const  muduo::net::TimerCallback & bCb,
    const  muduo::net::TimerCallback & eCb
)
{
    TimerDurationPtr ptr;

    switch (nType)
    {

    case E_NORMAL:
    {
        ptr = std::make_unique<TimerDuration>(nBeginTime, nEndTime, bCb, eCb);
    }
    break;
    case E_DURATION_DAILY:
    {
        ptr = std::make_unique<TimerDuration>(nBeginTime, nEndTime, bCb, eCb);
    }
    break;
    case  E_DURATION_WEEK:
    {
        ptr = std::make_unique<TimerDuration>(nBeginTime, nEndTime, bCb, eCb);
    }
    break;
    default:
        break;
    }

    ptr->CalcBeginEndTime(now);

    return ptr;
}