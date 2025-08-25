#pragma once

#include "util/game_registry.h"

class  NpcSystem
{
public:
    static void InitializeNpcComponents(entt::entity npc);

    static void CreateNpc();
};