#pragma once

#include <memory>
#include <unordered_map>
#include "time/comp/timer_task_comp.h"
#include "proto/common/component/buff_comp.pb.h"

using BuffMessagePtr = std::shared_ptr<google::protobuf::Message>;

struct BuffEntry
{
    BuffComp buffPb;
    TimerTaskComp expireTimerTaskComp;
    BuffMessagePtr dataPbPtr;
};

struct BuffListComp : std::unordered_map<uint64_t, BuffEntry>
{
    using unordered_map::unordered_map;
};

// Tag component: present when the entity has at least one active stealth buff.
// Maintained by BuffSystem::OnBuffStart / OnBuffRemove to avoid per-tick
// linear scan + table lookup in ViewSystem::IsStealthed.
struct StealthedTagComp {};
