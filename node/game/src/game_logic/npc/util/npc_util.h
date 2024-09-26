#pragma once

#include "util/game_registry.h"

class  NpcUtil
{
public:
    static void InitializeNpcComponents(entt::entity entity);

    static void CreateNpc();
};