#include "actor_state_attribute_sync_util.h"

// 引入必要的头文件
#include "client_player/player_state_attribute_sync.pb.h"
#include "component/actor_comp.pb.h"
#include "component/actor_status_comp.pb.h"
#include "game_logic/actor/constants/actor_state_attribute_constants.h"
#include "game_logic/network/message_util.h"
#include "game_logic/scene/comp/scene_comp.h"
#include "game_logic/scene/util/view_util.h"
#include "grpc/async_client_call.h"
#include "service_info/player_state_attribute_sync_service_info.h"
#include "thread_local/storage.h"

// 初始化同步工具类，当前没有初始化逻辑
void ActorStateAttributeSyncUtil::Initialize() {
    // 可添加全局初始化逻辑
}

// 初始化实体的组件
void ActorStateAttributeSyncUtil::InitializeActorComponents(const entt::entity entity) {
    // 在实体上添加速度、计算属性、派生属性和同步消息组件
    tls.registry.emplace<Velocity>(entity);
    tls.registry.emplace<CalculatedAttributesPbComponent>(entity);
    tls.registry.emplace<DerivedAttributesPbComponent>(entity);
    tls.registry.emplace<BaseAttributeDeltaS2C>(entity);
    tls.registry.emplace<AttributeDelta2FramesS2C>(entity);
    tls.registry.emplace<AttributeDelta5FramesS2C>(entity);
    tls.registry.emplace<AttributeDelta10FramesS2C>(entity);
    tls.registry.emplace<AttributeDelta30FramesS2C>(entity);
    tls.registry.emplace<AttributeDelta60FramesS2C>(entity);
}

// 获取附近一级实体列表
void ActorStateAttributeSyncUtil::GetNearbyLevel1Entities(const entt::entity entity, EntityVector& nearbyEntities) {
    const auto& aoiList = tls.registry.get<AoiListComp>(entity).aoiList;

    for (const auto& nearbyEntity : aoiList) {
        constexpr double viewRadiusFactor = 0.333;
        const double viewRadius = ViewUtil::GetMaxViewRadius(nearbyEntity) * viewRadiusFactor;

        if (!ViewUtil::IsWithinViewRadius(nearbyEntity, entity, viewRadius)) {
            continue;
        }

        nearbyEntities.emplace_back(nearbyEntity);
    }
}

// 获取附近二级实体列表
void ActorStateAttributeSyncUtil::GetNearbyLevel2Entities(const entt::entity entity, EntityVector& nearbyEntities) {
    const auto& aoiList = tls.registry.get<AoiListComp>(entity).aoiList;

    for (const auto& nearbyEntity : aoiList) {
        constexpr double viewRadiusFactor = 0.666;
        const double viewRadius = ViewUtil::GetMaxViewRadius(nearbyEntity) * viewRadiusFactor;

        if (!ViewUtil::IsWithinViewRadius(nearbyEntity, entity, viewRadius)) {
            continue;
        }

        nearbyEntities.emplace_back(nearbyEntity);
    }
}

// 获取附近三级实体列表
void ActorStateAttributeSyncUtil::GetNearbyLevel3Entities(const entt::entity entity, EntityVector& nearbyEntities) {
    const auto& aoiList = tls.registry.get<AoiListComp>(entity).aoiList;

    for (const auto& nearbyEntity : aoiList) {
        const double viewRadius = ViewUtil::GetMaxViewRadius(nearbyEntity);

        if (!ViewUtil::IsWithinViewRadius(nearbyEntity, entity, viewRadius)) {
            continue;
        }

        nearbyEntities.emplace_back(nearbyEntity);
    }
}

// 同步基础属性到附近的实体
void ActorStateAttributeSyncUtil::SyncBasicAttributes(entt::entity entity) {
    const auto aoiListComp = tls.registry.try_get<AoiListComp>(entity);
    if (aoiListComp == nullptr) {
        return;
    }

    // 获取当前实体的增量同步消息
    auto& syncMessage = tls.registry.get<BaseAttributeDeltaS2C>(entity);
    BroadCastToPlayer(EntitySyncServiceSyncBaseAttributeMessageId, syncMessage, aoiListComp->aoiList);

    // 发送后清空消息，准备下一次增量数据
    syncMessage.Clear();
}

// 同步属性，根据频率决定同步内容
void ActorStateAttributeSyncUtil::SyncAttributes(entt::entity entity, const EntityVector& nearbyEntities, uint32_t syncFrequency) {
    for (const auto& nearbyEntity : nearbyEntities) {
        switch (syncFrequency) {
            case eAttributeSyncFrequency::kSyncEvery2Frames:
                {
                    auto& syncMessage = tls.registry.get<AttributeDelta2FramesS2C>(entity);
                    
                    if (syncMessage.ByteSizeLong() <= 0)
                    {
                        return;
                    }
                    
                    BroadCastToPlayer(EntitySyncServiceSyncAttribute2FramesMessageId, syncMessage, nearbyEntities);
                    
                    syncMessage.Clear();
                }
                break;

            case eAttributeSyncFrequency::kSyncEvery5Frames:
                {
                    auto& syncMessage = tls.registry.get<AttributeDelta5FramesS2C>(entity);
                    
                    if (syncMessage.ByteSizeLong() <= 0)
                    {
                        return;
                    }
                    
                    BroadCastToPlayer(EntitySyncServiceSyncAttribute5FramesMessageId, syncMessage, nearbyEntities);
                    
                    syncMessage.Clear();
                }
                break;

            case eAttributeSyncFrequency::kSyncEvery10Frames:
                {
                    auto& syncMessage = tls.registry.get<AttributeDelta10FramesS2C>(entity);

                    if (syncMessage.ByteSizeLong() <= 0)
                    {
                        return;
                    }
                    
                    BroadCastToPlayer(EntitySyncServiceSyncAttribute10FramesMessageId, syncMessage, nearbyEntities);
                    
                    syncMessage.Clear();
                }
                break;

            case eAttributeSyncFrequency::kSyncEvery30Frames:
                {
                    auto& syncMessage = tls.registry.get<AttributeDelta30FramesS2C>(entity);
                    
                    if (syncMessage.ByteSizeLong() <= 0)
                    {
                        return;
                    }
                    
                    BroadCastToPlayer(EntitySyncServiceSyncAttribute30FramesMessageId, syncMessage, nearbyEntities);
                    
                    syncMessage.Clear();
                }
                break;

            case eAttributeSyncFrequency::kSyncEvery60Frames:
                {
                    auto& syncMessage = tls.registry.get<AttributeDelta60FramesS2C>(entity);

                    if (syncMessage.ByteSizeLong() <= 0)
                    {
                        return;
                    }
                    
                    BroadCastToPlayer(EntitySyncServiceSyncAttribute60FramesMessageId, syncMessage, nearbyEntities);
                    
                    syncMessage.Clear();
                }
                break;

            default:
                // 其他情况处理
                break;
        }
    }
}
