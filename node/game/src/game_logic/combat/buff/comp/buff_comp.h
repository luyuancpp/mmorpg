#pragma once

#include <cstdint>
#include <memory>
#include "logic/component/buff_comp.pb.h"
#include "game_logic/combat/ability/comp/skill_comp.h"
#include "time/comp/timer_task_comp.h"


struct BuffComp
{
    BuffPBComp buffPB;
    SkillContextPtrComp abilityContext;
    TimerTaskComp intervalTTimer;
};

using BuffList = std::map<uint64_t, BuffComp>;
struct BuffListComp
{
    BuffList buffList;
};