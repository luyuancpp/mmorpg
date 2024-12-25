﻿#pragma once
#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

class TeamSystem
{
public:
    static uint32_t TryCreateTeam(entt::entity actorEntity);

    static uint32_t InitTeamInfo(entt::entity actorEntity);
};
