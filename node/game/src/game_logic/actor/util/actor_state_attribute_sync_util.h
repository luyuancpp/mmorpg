#pragma once

#include "bag/item_util.h"
#include "util/game_registry.h"

class ActorStateAttributeSyncUtil
{
public:
    static void Initialize();
    
    static void InitializeActorComponents(entt::entity entity);

    static void GetNearbyLevel1Entities(const entt::entity entity, EntityVector& entityList);

    static void GetNearbyLevel2Entities(const entt::entity entity, EntityVector& entityList);

    static void GetNearbyLevel3Entities(const entt::entity entity, EntityVector& entityList);

    static void SyncBasicAttributes(entt::entity entity);

    static void SyncAttributes(entt::entity entity, const EntityVector& nearbyEntities, uint32_t syncFrequency);
    
};