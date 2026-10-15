#pragma once
#include <cstdint>
#include <vector>
#include "entt/src/entt/entity/entity.hpp"

// player 属于 actorRegistry;事实只在本进程 loop 内传递,不序列化。
struct AcceptMissionEvent { entt::entity player{entt::null}; uint32_t missionId{0}; };
struct OnAcceptedMissionEvent { entt::entity player{entt::null}; uint32_t missionId{0}; };
struct OnMissionAwardEvent { entt::entity player{entt::null}; uint32_t missionId{0}; };
struct ConditionEvent
{
    entt::entity player{entt::null};
    uint32_t conditionType{0};
    std::vector<uint32_t> conditionIds;
    uint32_t amount{0};
};
