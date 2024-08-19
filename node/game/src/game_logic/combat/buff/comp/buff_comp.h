#pragma once

#include <cstdint>
#include <memory>
#include "logic/component/buff_comp.pb.h"


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
};

struct BuffListComp
{
    std::map<uint32_t, BuffComp> buffList;
};