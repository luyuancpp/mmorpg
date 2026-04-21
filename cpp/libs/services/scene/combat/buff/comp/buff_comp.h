#pragma once

#include <memory>
#include <unordered_map>
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

using BuffListComp = std::unordered_map<uint64_t, BuffEntry>;
using BuffPendingRemoveBuffs = std::unordered_set<uint64_t>;

// Tag component: present when the entity has at least one active stealth buff.
// Maintained by BuffSystem::OnBuffStart / OnBuffRemove to avoid per-tick
// linear scan + table lookup in ViewSystem::IsStealthed.
struct StealthedTagComp {};