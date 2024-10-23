#include "actor_state_attribute_sync_util.h"

#include "client_player/player_state_attribute_sync.pb.h"
#include "component/actor_comp.pb.h"
#include "component/actor_status_comp.pb.h"
#include "game_logic/actor/constants/actor_state_attribute_constants.h"
#include "game_logic/network/message_util.h"
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
    tls.registry.emplace<Velocity>(entity);  // 速度组件
    tls.registry.emplace<CalculatedAttributesPBComponent>(entity);  // 计算的属性组件
    tls.registry.emplace<DerivedAttributesPBComponent>(entity);  // 派生属性组件
    tls.registry.emplace<BaseAttributeDeltaS2C>(entity);  // 属性增量同步消息组件
    tls.registry.emplace<AttributeDelta2FramesS2C>(entity);
    tls.registry.emplace<AttributeDelta5FramesS2C>(entity);
    tls.registry.emplace<AttributeDelta10FramesS2C>(entity);
    tls.registry.emplace<AttributeDelta30FramesS2C>(entity);
    tls.registry.emplace<AttributeDelta60FramesS2C>(entity);
}

// 获取1级范围内的附近实体列表
void ActorStateAttributeSyncUtil::GetNearLevel1EntityList(const entt::entity entity, EntityVector& entityList) {
    // TODO: 实现1级范围内的实体查找逻辑
}

// 获取2级范围内的附近实体列表
void ActorStateAttributeSyncUtil::GetNearLevel2EntityList(const entt::entity entity, EntityVector& entityList) {
    // TODO: 实现2级范围内的实体查找逻辑
}

// 获取3级范围内的附近实体列表
void ActorStateAttributeSyncUtil::GetNearLevel3EntityList(const entt::entity entity, EntityVector& entityList) {
    // TODO: 实现3级范围内的实体查找逻辑
}

// 同步基础属性到附近的实体
void ActorStateAttributeSyncUtil::SyncBasicAttributes(entt::entity entity, const EntityVector& nearbyEntities, double delta) {
    // 获取当前实体的增量同步消息
    auto& message = tls.registry.get<BaseAttributeDeltaS2C>(entity);

    BroadCastToPlayer(EntitySyncServiceSyncBaseAttributeMessageId, message, nearbyEntities);

    // 发送后清空消息，准备下一次增量数据
    message.Clear();
}

// 同步属性，根据频率决定同步内容
void ActorStateAttributeSyncUtil::SyncAttributes(entt::entity entity, const EntityVector& nearbyEntities, uint32_t syncFrequency, double delta) {
    for (const auto& nearbyEntity : nearbyEntities) {
        // 根据不同的同步频率执行不同的同步逻辑
        switch (syncFrequency) {
            case eAttributeSyncFrequency::kSyncEvery2Frames:
                {
                    auto& message = tls.registry.get<AttributeDelta2FramesS2C>(entity);

                    BroadCastToPlayer(EntitySyncServiceSyncAttribute2FramesMessageId, message, nearbyEntities);

                    message.Clear();
                }
                break;

            case eAttributeSyncFrequency::kSyncEvery5Frames:
                {
                    auto& message = tls.registry.get<AttributeDelta5FramesS2C>(entity);

                    BroadCastToPlayer(EntitySyncServiceSyncAttribute5FramesMessageId, message, nearbyEntities);

                    message.Clear();
                }
                break;

            case eAttributeSyncFrequency::kSyncEvery10Frames:
                {
                    auto& message = tls.registry.get<AttributeDelta10FramesS2C>(entity);

                    BroadCastToPlayer(EntitySyncServiceSyncAttribute10FramesMessageId, message, nearbyEntities);

                    message.Clear();
                }
            break;

            case eAttributeSyncFrequency::kSyncEvery30Frames:
                {
                    auto& message = tls.registry.get<AttributeDelta30FramesS2C>(entity);

                    BroadCastToPlayer(EntitySyncServiceSyncAttribute30FramesMessageId, message, nearbyEntities);

                    message.Clear();
                }
            break;

            case eAttributeSyncFrequency::kSyncEvery60Frames:
                {
                    auto& message = tls.registry.get<AttributeDelta60FramesS2C>(entity);

                    BroadCastToPlayer(EntitySyncServiceSyncAttribute60FramesMessageId, message, nearbyEntities);

                    message.Clear();
                }
            break;

            default:
                // 其他情况处理
                break;
        }
    }
}
