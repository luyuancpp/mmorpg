#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

struct BuffListComp : std::unordered_map<uint64_t, BuffEntry>
{
    using Base = std::unordered_map<uint64_t, BuffEntry>;
    using Base::Base;
    BuffListComp() = default;
    explicit BuffListComp(const Base& values) : Base(values) {}
    explicit BuffListComp(Base&& values) : Base(std::move(values)) {}
};

struct BuffPendingRemoveBuffs : std::unordered_set<uint64_t>
{
    using Base = std::unordered_set<uint64_t>;
    using Base::Base;
    BuffPendingRemoveBuffs() = default;
    explicit BuffPendingRemoveBuffs(const Base& values) : Base(values) {}
    explicit BuffPendingRemoveBuffs(Base&& values) : Base(std::move(values)) {}
};

// Tag component: present when the entity has at least one active stealth buff.
// Maintained by BuffSystem::OnBuffStart / OnBuffRemove to avoid per-tick
// linear scan + table lookup in ViewSystem::IsStealthed.
struct StealthedTagComp {};
