#pragma once

#include <cstdint>
#include <memory>
#include "combat/skill/comp/skill_comp.h"
#include "proto/common/component/buff_comp.pb.h"

using BuffMessagePtr = std::shared_ptr<google::protobuf::Message>;

struct BuffEntry
{
    BuffComp buffPb;
    SkillContextPtrComp skillContext;
    TimerTaskComp expireTimerTaskComp;
    BuffMessagePtr dataPbPtr;
};

using BuffListComp = std::map<uint64_t, BuffEntry>;
using BuffPendingRemoveBuffs = std::unordered_set<uint64_t>;