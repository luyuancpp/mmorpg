#pragma once

#include <cstdint>
#include <memory>
#include "scene/combat/skill/comp/skill.h"
#include "proto/logic/component/buff_comp.pb.h"

using BuffMessagePtr = std::shared_ptr<google::protobuf::Message>;

struct BuffComp
{
    BuffPbComponent buffPb;
    SkillContextPtrComp skillContext;
    TimerTaskComp expireTimerTaskComp;
    BuffMessagePtr dataPbPtr;
};

using BuffListComp = std::map<uint64_t, BuffComp>;
using BuffPendingRemoveBuffs = std::unordered_set<uint64_t>;