#include "actor_state_attribute_sync_system.h"

#include "component/actor_comp.pb.h"
#include "game_logic/actor/constants/actor_state_attribute_constants.h"
#include "game_logic/actor/util/actor_state_attribute_sync_util.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"

void ActorStateAttributeSyncSystem::Update(const double delta)
{
    const auto frameCount = tlsGame.frameTime.frame_count();

    EntityVector entityList;

    // 遍历所有带有 Transform 组件的实体
    for (auto [entity, transform] : tls.registry.view<Transform>().each())
    {
        // 获取 Level 1 近距离的实体，并进行高频同步
        ActorStateAttributeSyncUtil::GetNearLevel1EntityList(entity, entityList);
        ActorStateAttributeSyncUtil::SyncAttributes(entity, entityList, eAttributeSyncFrequency::kSyncEvery1Frame, delta);
        entityList.clear();  // 清空列表

        // 获取 Level 2 中距离的实体，并每 2 帧同步一次
        ActorStateAttributeSyncUtil::GetNearLevel2EntityList(entity, entityList);
        if (frameCount % eAttributeSyncFrequency::kSyncEvery2Frames == 0) {
            ActorStateAttributeSyncUtil::SyncAttributes(entity, entityList, eAttributeSyncFrequency::kSyncEvery2Frames, delta);
        }
        entityList.clear();

        // 获取 Level 3 远距离的实体，并每 5 帧同步一次
        ActorStateAttributeSyncUtil::GetNearLevel3EntityList(entity, entityList);
        if (frameCount % eAttributeSyncFrequency::kSyncEvery5Frames == 0) {
            ActorStateAttributeSyncUtil::SyncAttributes(entity, entityList, eAttributeSyncFrequency::kSyncEvery5Frames, delta);
        }
        entityList.clear();

        // 每 10 帧同步一次低优先级属性
        if (frameCount % eAttributeSyncFrequency::kSyncEvery10Frames == 0) {
            ActorStateAttributeSyncUtil::SyncAttributes(entity, entityList, eAttributeSyncFrequency::kSyncEvery10Frames, delta);
        }

        // 每 30 帧同步一次很低优先级属性
        if (frameCount % eAttributeSyncFrequency::kSyncEvery30Frames == 0) {
            ActorStateAttributeSyncUtil::SyncAttributes(entity, entityList, eAttributeSyncFrequency::kSyncEvery30Frames, delta);
        }
    }
}

