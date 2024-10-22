#pragma once

#include "bag/item_util.h"
#include "util/game_registry.h"

class ActorStateAttributeSyncUtil
{
public:
    static void Initialize();
    
    static void InitializeActorComponents(entt::entity entity);

    static void GetNearLevel1EntityList(entt::entity entity, const EntityVector& entityList);

    static void GetNearLevel2EntityList(entt::entity entity, const EntityVector& entityList);

    static void GetNearLevel3EntityList(entt::entity entity, const EntityVector& entityList);

    static void SyncAttributes(entt::entity entity, const EntityVector& nearbyEntities, uint32_t syncFrequency, double delta);
    
};