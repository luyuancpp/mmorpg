#include "timer_task_comp.h"
#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"

#include <cassert>

// EventLoop destructor sets t_loopInThisThread = NULL before entt registry
// cleanup destroys components, so every call site must tolerate nullptr.
static inline muduo::net::EventLoop* GetThreadEventLoop() {
    return muduo::net::EventLoop::getEventLoopOfCurrentThread();
}

// ---- Lifecycle --------------------------------------------------------------

// Default ctor: timerId and callback are already value-initialized.
TimerTaskComp::TimerTaskComp() = default;

TimerTaskComp::~TimerTaskComp() {
    Cancel();
}

// Copy ctor: intentionally does NOT copy the timer. The timer callback
// captures `this`, so sharing it across instances would create a dangling
// pointer. The copy starts empty; the source keeps its timer.
TimerTaskComp::TimerTaskComp(const TimerTaskComp& /*unused*/) {}

// Move ctor: cancel the source's timer (its callback captures source's
// `this` which will be in a moved-from state). Destination starts empty.
TimerTaskComp::TimerTaskComp(TimerTaskComp&& param) noexcept {
    param.Cancel();
}

TimerTaskComp& TimerTaskComp::operator=(TimerTaskComp&& param) noexcept {
    if (this != &param) {
        Cancel();        // release our own timer first
        param.Cancel();  // then invalidate the source
    }
    return *this;
}

// ---- Scheduling -------------------------------------------------------------

template <typename ScheduleFn>
void TimerTaskComp::ScheduleTimer(const TimerCallback& cb, ScheduleFn&& schedule) {
    Cancel();

    if (!cb) {
        return;
    }

    auto* loop = GetThreadEventLoop();
    if (loop == nullptr) {
        return;
    }

    callback = cb;
    timerId = schedule(loop, std::bind(&TimerTaskComp::OnTimer, this));
}

void TimerTaskComp::RunAt(const Timestamp& time, const TimerCallback& cb) {
    ScheduleTimer(cb, [&](EventLoop* loop, TimerCallback bound) {
        return loop->runAt(time, std::move(bound));
    });
}

void TimerTaskComp::RunAfter(double delay, const TimerCallback& cb) {
    ScheduleTimer(cb, [&](EventLoop* loop, TimerCallback bound) {
        return loop->runAfter(delay, std::move(bound));
    });
}

void TimerTaskComp::RunEvery(double interval, const TimerCallback& cb) {
    ScheduleTimer(cb, [&](EventLoop* loop, TimerCallback bound) {
        return loop->runEvery(interval, std::move(bound));
    });
}

// ---- Execution & cancellation -----------------------------------------------

void TimerTaskComp::Run() const {
    if (callback) {
        callback();
    }
}

void TimerTaskComp::Cancel() {
    // Null-check required: EventLoop may already be destroyed during shutdown.
    auto* loop = GetThreadEventLoop();
    if (loop != nullptr) {
        loop->cancel(timerId);
    }
    timerId = TimerId();
    callback = TimerCallback();
    assert(timerId.GetTimer() == nullptr);
}

// ---- Query ------------------------------------------------------------------

bool TimerTaskComp::IsActive() const {
    auto* timer = timerId.GetTimer();
    if (!timer) {
        return false;
    }

    auto expiration = timer->expiration();
    return !(expiration == Timestamp::invalid()) && !(expiration < Timestamp::now());
}

uint64_t TimerTaskComp::GetEndTime() const {
    auto* timer = timerId.GetTimer();
    if (!timer) {
        return 0;
    }

    return timer->expiration().secondsSinceEpoch();
}

void TimerTaskComp::SetCallBack(const TimerCallback& cb) {
    callback = cb;
}

// ---- Internal ---------------------------------------------------------------

void TimerTaskComp::OnTimer() {
    auto* timer = timerId.GetTimer();
    const bool repeating = timer && timer->repeat();

    if (!repeating) {
        timerId = TimerId();
    }

    if (callback) {
        // Copy before invoking: the callback may re-schedule or cancel this timer,
        // which would overwrite `callback` while we're still using it.
        const TimerCallback localCb = callback;
        localCb();
    }
}
