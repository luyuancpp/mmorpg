#pragma once

//https://youxiputao.com/article/24673.html
//https://dev.epicgames.com/documentation/zh-cn/unreal-engine/replication-graph-in-unreal-engine

#include "engine/core/type_define/type_define.h"

class ActorStateAttributeSyncSystem
{
public:
    static void Update(double delta);

    static void GetNearbyLevel1Entities(const entt::entity entity, EntityVector& nearbyEntities);

    static void GetNearbyLevel2Entities(const entt::entity entity, EntityVector& nearbyEntities);

    static void GetNearbyLevel3Entities(const entt::entity entity, EntityVector& nearbyEntities);

    static void SyncAttributes(entt::entity entity, const EntityVector& nearbyEntities, uint32_t syncFrequency);
};