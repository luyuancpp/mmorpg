#pragma once
#include "time/comp/timer_task_comp.h"
#include "proto/common/component/skill_comp.pb.h"


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
struct ChannelIntervalTimerComp
{
    TimerTaskComp timer;
};

using SkillContextPtrComp = std::shared_ptr<SkillContextPBComponent>;

// 存储技能上下文信息的容器
using  SkillContextCompMap = std::unordered_map<uint64_t, SkillContextPtrComp>;