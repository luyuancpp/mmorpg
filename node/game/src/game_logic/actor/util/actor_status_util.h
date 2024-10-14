#pragma once

#include "util/game_registry.h"

class ActorStatusUtil
{
public:
    static void Initialize();
    
    static void InitializeActorComponents(entt::entity entity);
};