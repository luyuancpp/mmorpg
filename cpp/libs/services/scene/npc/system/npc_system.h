#pragma once

#include "core/utils/registry/game_registry.h"

class  NpcSystem
{
public:
    static void InitializeNpcComponents(entt::entity npc);

    static void CreateNpc();
};