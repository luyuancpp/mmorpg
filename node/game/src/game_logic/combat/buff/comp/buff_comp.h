#pragma once

#include <cstdint>
#include <memory>
#include "logic/component/buff_comp.pb.h"
#include "time/comp/timer_task_comp.h"


struct BuffAbilityContextComp
{
    uint32_t tableAbilityId = { 0 };
    uint64_t caster = { entt::null };
};

using BuffAbilityContextPtrComp = std::shared_ptr<BuffAbilityContextComp>;

struct BuffComp
{
    BuffPBComp buffPB;
    BuffAbilityContextPtrComp abilityContext;
    TimerTaskComp intervalTTimer;
};

struct BuffListComp
{
    std::map<uint64_t, BuffComp> buffList;
};