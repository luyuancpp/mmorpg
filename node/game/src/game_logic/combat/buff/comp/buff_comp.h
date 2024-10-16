#pragma once

#include <cstdint>
#include <memory>
#include "game_logic/combat/skill/comp//skill_comp.h"
#include "logic/component/buff_comp.pb.h"

struct BuffComp
{
    BuffPbComponent buffPb;
    SkillContextPtrComp abilityContext;
    PeriodicBuffPbComponent periodicBuffPbComponent;
};

using BuffListComp = std::map<uint64_t, BuffComp>;