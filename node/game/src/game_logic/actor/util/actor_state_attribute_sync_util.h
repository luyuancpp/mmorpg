#pragma once

#include "util/game_registry.h"

class ActorStateAttributeSyncUtil
{
public:
    static void Initialize();
    
    static void InitializeActorComponents(entt::entity entity);
};