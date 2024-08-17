#pragma once
#include "time/comp/timer_task_comp.h"

//前摇技能timer
struct CastingTimerComp
{
    TimerTaskComp timer;
};

//后摇技能timer
struct RecoveryTimerComp
{
    TimerTaskComp timer;
};

//引导技能timer
struct ChannelFinishTimerComp
{
    TimerTaskComp timer;
};
//引导技能timer
struct ChannelThinkIntervalTimerComp
{
    TimerTaskComp timer;
};