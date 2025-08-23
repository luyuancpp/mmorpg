#pragma once

//https://youxiputao.com/article/24673.html
//https://dev.epicgames.com/documentation/zh-cn/unreal-engine/replication-graph-in-unreal-engine
//https://zhuanlan.zhihu.com/p/34723199

#include "type_define/type_define.h"
#include "util/game_registry.h"

class ActorStateAttributeSyncSystem
{
public:
    static void Update(double delta);

    static void Initialize();

    static void InitializeActorComponents(entt::entity entity);

    static void GetNearbyLevel1Entities(const entt::entity entity, EntityVector& nearbyEntities);

    static void GetNearbyLevel2Entities(const entt::entity entity, EntityVector& nearbyEntities);

    static void GetNearbyLevel3Entities(const entt::entity entity, EntityVector& nearbyEntities);

    static void SyncBasicAttributes(entt::entity entity);

    static void SyncAttributes(entt::entity entity, const EntityVector& nearbyEntities, uint32_t syncFrequency);
};