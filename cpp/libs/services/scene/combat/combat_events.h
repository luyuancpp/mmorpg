#pragma once
#include <cstdint>
#include "entt/src/entt/entity/entity.hpp"

// caster/target 属于 actorRegistry;只在本进程的技能与 Buff 调用链上传递。
struct SkillExecutedEvent
{
    uint64_t skillId{0};
    uint32_t skillTableId{0};
    entt::entity caster{entt::null};
    entt::entity target{entt::null};
};
