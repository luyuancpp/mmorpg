#pragma once
#include <cstdint>
#include <vector>
#include "entt/src/entt/entity/entity.hpp"

// player 属于 actorRegistry;只作本进程 loop 的直接调用参数,不经 dispatcher,不序列化。
struct AcceptMissionEvent { entt::entity player{entt::null}; uint32_t missionId{0}; };
struct ConditionEvent
{
    entt::entity player{entt::null};
    uint32_t conditionType{0};
    std::vector<uint32_t> conditionIds;
    uint32_t amount{0};
};
