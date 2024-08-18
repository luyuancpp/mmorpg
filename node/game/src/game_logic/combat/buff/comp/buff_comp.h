#pragma once

#include <cstdint>
#include <memory>
#include "logic/component/buff_comp.pb.h"

struct BuffAbilityContextComp
{
    uint32_t tableAbilityId = { 0 };
};

using BuffAbilityContextPtrComp = std::shared_ptr<BuffAbilityContextComp>;

struct BuffListComp
{
    std::map<uint32_t, BuffPBComp> buffList;
};