#pragma once
#include "time/comp/timer_task_comp.h"
#include "proto/common/component/skill_comp.pb.h"


// Pre-cast timer
struct CastingTimerComp
{
    TimerTaskComp timer;
};

// Post-cast recovery timer
struct RecoveryTimerComp
{
    TimerTaskComp timer;
};

// Channel finish timer
struct ChannelFinishTimerComp
{
    TimerTaskComp timer;
};
// Channel interval timer
struct ChannelIntervalTimerComp
{
    TimerTaskComp timer;
};

using SkillContextPtrComp = std::shared_ptr<SkillContextPBComponent>;

// Skill context container
using  SkillContextCompMap = std::unordered_map<uint64_t, SkillContextPtrComp>;