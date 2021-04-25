#include "timer_task.h"

#include "muduo/net/TimerId.h"
#include "muduo/net/Timer.h"


namespace common
{
#define  gs_thread_even_loop EventLoop::getEventLoopOfCurrentThread()

	TimerTask::~TimerTask()
	{
        Cancel();
	}

	void TimerTask::RunAt(const Timestamp& time, const TimerCallback& cb)
	{
		Cancel();
        m_oCallBack = cb;
		m_oTimerId = gs_thread_even_loop->runAt(time, std::bind(&TimerTask::OnTimer, this));
        UpdateEndTeamDate();
	}

	void TimerTask::RunAfter(double delay, const TimerCallback& cb)
	{
		Cancel();
        m_oCallBack = cb;
        m_oTimerId = gs_thread_even_loop->runAfter(delay, std::bind(&TimerTask::OnTimer, this));
        UpdateEndTeamDate();
	}

	void TimerTask::RunEvery(double interval, const TimerCallback& cb)
	{
        Cancel();	
        m_oCallBack = cb;
        m_oTimerId = gs_thread_even_loop->runEvery(interval, std::bind(&TimerTask::OnTimer, this));
        UpdateEndTeamDate();
	}

    void TimerTask::Call()
    {
        if (!m_oCallBack)
        {
            return;
        }
        m_oCallBack();
    }

	void TimerTask::Cancel()
	{
        gs_thread_even_loop->cancel(m_oTimerId);
        m_oTimerId = TimerId();
        m_oEndTime = Timestamp();
#ifdef __TEST__
        assert(!m_oTimerId.GetTimer());
#endif // __TEST__
	}

    bool TimerTask::ActiveTimer()
    {
        return !(m_oEndTime.invalid() == m_oEndTime);
    }

    int32_t TimerTask::GetEndTime()
    {
        if (m_oEndTime < Timestamp::now() )
        {
            return 0;
        }
        return (int32_t)m_oTimerId.GetTimer()->expiration().secondsSinceEpoch();
    }

    void TimerTask::UpdateEndTeamDate()
    {
        if (nullptr != m_oTimerId.GetTimer())
        {
            m_oEndTime = m_oTimerId.GetTimer()->expiration();
        }
    }

    void TimerTask::SetCallBack(const TimerCallback& cb)
    {
        m_oCallBack = cb;
    }

    void TimerTask::OnTimer()
    {
        if (!m_oCallBack)
        {
            return;
        }
        TimerCallback copycb = m_oCallBack;
        copycb();
        m_oEndTime = Timestamp();
    }
}// namespace common