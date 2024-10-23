#include "actor_state_attribute_sync_util.h"

#include "client_player/player_state_attribute_sync.pb.h"
#include "component/actor_comp.pb.h"
#include "component/actor_status_comp.pb.h"
#include "game_logic/actor/constants/actor_state_attribute_constants.h"
#include "game_logic/network/message_util.h"
#include "grpc/async_client_call.h"
#include "service_info/player_state_attribute_sync_service_info.h"
#include "thread_local/storage.h"

class Acceleration;

void ActorStateAttributeSyncUtil::Initialize(){
    
}

void ActorStateAttributeSyncUtil::InitializeActorComponents(const entt::entity entity){
    tls.registry.emplace<Velocity>(entity);
    tls.registry.emplace<CalculatedAttributesPBComponent>(entity);
    tls.registry.emplace<DerivedAttributesPBComponent>(entity);
    tls.registry.emplace<SyncBaseStateAttributeDeltaS2C>(entity);
}

void ActorStateAttributeSyncUtil::GetNearLevel1EntityList(const entt::entity entity, EntityVector& entityList)
{
}

void ActorStateAttributeSyncUtil::GetNearLevel2EntityList(const entt::entity entity, EntityVector& entityList)
{
}

void ActorStateAttributeSyncUtil::GetNearLevel3EntityList(const entt::entity entity, EntityVector& entityList)
{
}

void ActorStateAttributeSyncUtil::SyncBasicAttributes(entt::entity entity, const EntityVector& nearbyEntities,
    double delta)
{
    auto& message = tls.registry.get<SyncBaseStateAttributeDeltaS2C>(entity);
    SendMessageToPlayer(EntityStateSyncServiceSyncBaseStateAttributeMessageId, message, entity);
    message.Clear();
}

void ActorStateAttributeSyncUtil::SyncAttributes(entt::entity entity, const EntityVector& nearbyEntities, uint32_t syncFrequency, double delta)
{
    for (const auto& nearbyEntity : nearbyEntities)
    {
        // 根据同步频率来决定同步逻辑
        switch (syncFrequency) {

        case eAttributeSyncFrequency::kSyncEvery2Frames:
            break;

        case eAttributeSyncFrequency::kSyncEvery5Frames:
            break;

        case eAttributeSyncFrequency::kSyncEvery10Frames:
            break;

        case eAttributeSyncFrequency::kSyncEvery30Frames:
            // 长时间同步一次，适用于非关键属性
            break;
        case eAttributeSyncFrequency::kSyncEvery60Frames:
            // 长时间同步一次，适用于非关键属性
                break;
        default:
            break;
        }
    }
}

