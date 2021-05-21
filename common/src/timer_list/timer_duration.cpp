#include "TimerDuration.h"

#include "DurationFuntion.h"

TimerDuration::TimerDuration(time_t nBeginTime, time_t nEndTime)
    : TimerDuration(nBeginTime, nEndTime, kEmptyCallBack, kEmptyCallBack)
{
}

TimerDuration::TimerDuration(time_t nBeginTime, time_t nEndTime, const muduo::TimerCallback & bCb, const muduo::TimerCallback & eCb)
    : m_nBeginTime(nBeginTime),
    m_nEndTime(nEndTime),
    m_bBeginCallBack(bCb),
    m_bEndCallBack(eCb)
{
}

TimerDuration::TimerDuration(const std::string & sBeginTime, const std::string & sEndTime, const muduo::TimerCallback & bCb, const muduo::TimerCallback & eCb)
    : TimerDuration(game::YmdHmsStringToTime(sBeginTime), game::YmdHmsStringToTime(sEndTime), bCb, eCb)
{
}

void TimerDuration::CalcBeginEndTime()
{
    CalcBeginEndTime(muduo::Timestamp::now().secondsSinceEpoch());
}

void TimerDuration::CalcBeginEndTime(time_t tNow)
{
    if (tNow <= m_nEndTime)
    {
		InitTimer();
    }
    else
    {
        CalcNextDuration();
    }
}

void TimerDuration::SetCallBack(const muduo::TimerCallback & bCb,
    const muduo::TimerCallback & eCb)
{
    m_bBeginCallBack = bCb;
    m_bEndCallBack = eCb;
}

void TimerDuration::InitTimer()
{
	m_oBeginTimer.runAt(muduo::Timestamp::fromUnixTime(m_nBeginTime), std::bind(&TimerDuration::OnBegin, this));
	m_oEndTimer.runAt(muduo::Timestamp::fromUnixTime(m_nEndTime), std::bind(&TimerDuration::OnEnd, this));
}

TimerDuration::time_duration_ptr TimerDuration::CreateDuration(int32_t nType,
    time_t now,
    const std::string & sBeginTime,
    const std::string & sEndTime,
    const muduo::TimerCallback & bCb,
    const muduo::TimerCallback & eCb)
{
    time_duration_ptr ptr;
    switch (nType)
    {

    case E_NORMAL:
    {
        return  CreateDuration(nType, now, sBeginTime, sEndTime, bCb, eCb);
    }
    break;
    case E_DURATION_DAILY:
    {
        muduo::Date dateToday(game::GetTodayDate(now));
        return  CreateDuration(nType, now, game::GetTime(dateToday, sBeginTime), game::GetTime(dateToday, sEndTime), bCb, eCb);
    }
    break;
    case  E_DURATION_WEEK:
    {
        muduo::Date dateToday(game::GetTodayDate(now));
        muduo::Date dateWeekBegin(game::GetWeekBeginDay(dateToday));
        return  CreateDuration(nType, now, game::GetWeekTime(dateWeekBegin, sBeginTime), game::GetWeekTime(dateWeekBegin, sEndTime), bCb, eCb);
    }
    break;
    default:
        break;
    }
    return ptr;
}

TimerDuration::time_duration_ptr TimerDuration::CreateDuration(int32_t nType,
    time_t now,
    time_t nBeginTime,
    time_t nEndTime,
    const muduo::TimerCallback & bCb,
    const muduo::TimerCallback & eCb
)
{
    time_duration_ptr ptr;

    switch (nType)
    {

    case E_NORMAL:
    {
        ptr.reset(new TimerDuration(nBeginTime, nEndTime, bCb, eCb));
    }
    break;
    case E_DURATION_DAILY:
    {
        ptr.reset(new DailyTimerDuration(nBeginTime, nEndTime, bCb, eCb));
    }
    break;
    case  E_DURATION_WEEK:
    {
        ptr.reset(new WeekTimerDuration(nBeginTime, nEndTime, bCb, eCb));
    }
    break;
    default:
        break;
    }

    ptr->CalcBeginEndTime(now);

    return ptr;
}