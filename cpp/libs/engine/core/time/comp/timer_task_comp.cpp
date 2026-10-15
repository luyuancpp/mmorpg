#include "timer_task_comp.h"

#include <utility>

#include "muduo/net/TimerId.h"

// EventLoop 可能先于线程局部 registry 销毁,取消时必须允许没有当前 loop。
static EventLoop* GetThreadEventLoop() {
    return EventLoop::getEventLoopOfCurrentThread();
}

TimerTaskComp::TimerTaskComp() = default;

TimerTaskComp::~TimerTaskComp() {
    Cancel();
}

// 复制得到未武装的新定时器,源对象继续独占原计时。
TimerTaskComp::TimerTaskComp(const TimerTaskComp& /*unused*/) {}

TimerTaskComp::TimerTaskComp(TimerTaskComp&& param) noexcept
    : state_(std::move(param.state_)) {}

TimerTaskComp& TimerTaskComp::operator=(TimerTaskComp&& param) noexcept {
    if (this != &param) {
        Cancel();
        state_ = std::move(param.state_);
    }
    return *this;
}

template <typename ScheduleFn>
void TimerTaskComp::ScheduleTimer(TimerCallback cb, bool repeating, ScheduleFn&& schedule) {
    Cancel();
    auto* loop = GetThreadEventLoop();
    if (!cb || loop == nullptr) {
        return;
    }
    if (!state_) {
        state_ = std::make_shared<State>();
    }

    const auto gen = state_->generation;
    state_->timerId = schedule(loop, [weakState = std::weak_ptr<State>(state_), gen, repeating,
                                      cb = std::move(cb)]() {
        // 先前的到期回调可能已经销毁或移动宿主;只访问保活后的独立状态。
        const auto state = weakState.lock();
        if (state) {
            OnTimer(*state, gen, repeating, cb);
        }
    });
    state_->armed = true;
}

void TimerTaskComp::RunAt(const Timestamp& time, const TimerCallback& cb) {
    ScheduleTimer(cb, false, [&](EventLoop* loop, TimerCallback bound) {
        return loop->runAt(time, std::move(bound));
    });
}

void TimerTaskComp::RunAfter(double delay, const TimerCallback& cb) {
    ScheduleTimer(cb, false, [&](EventLoop* loop, TimerCallback bound) {
        return loop->runAfter(delay, std::move(bound));
    });
}

void TimerTaskComp::RunEvery(double interval, const TimerCallback& cb) {
    ScheduleTimer(cb, true, [&](EventLoop* loop, TimerCallback bound) {
        return loop->runEvery(interval, std::move(bound));
    });
}

void TimerTaskComp::Cancel() {
    if (!state_) {
        return;
    }
    if (auto* loop = GetThreadEventLoop(); loop != nullptr && state_->armed) {
        loop->cancel(state_->timerId);
    }
    state_->timerId = TimerId();
    state_->armed = false;
    // cancel 无法保证移出已取出的到期批次,必须另外使旧开火失效。
    ++state_->generation;
}

bool TimerTaskComp::IsActive() const {
    return state_ && state_->armed;
}

void TimerTaskComp::OnTimer(State& state, uint32_t firedGeneration, bool repeating, const TimerCallback& cb) {
    if (firedGeneration != state.generation) {
        return;
    }
    if (!repeating) {
        state.timerId = TimerId();
        state.armed = false;
    }
    // cb 存在于 muduo 的闭包中,不存于 State;重入取消、重挂或宿主销毁都不会析构当前 callable。
    // cb 返回后不再访问宿主或状态,避免把新武装的计时改回旧值。
    cb();
}
