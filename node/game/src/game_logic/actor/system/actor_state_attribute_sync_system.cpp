#include "actor_state_attribute_sync_system.h"

#include "component/actor_comp.pb.h"
#include "game_logic/actor/constants/actor_state_attribute_constants.h"
#include "game_logic/actor/util/actor_state_attribute_sync_util.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"

constexpr uint32_t kFrameArraySize = 5;

using SyncLevelConfigs = std::array<uint32_t, kFrameArraySize>;

constexpr  SyncLevelConfigs syncConfigsLevel1{
     eAttributeSyncFrequency::kSyncEvery1Frame,
     eAttributeSyncFrequency::kSyncEvery2Frames, 
     eAttributeSyncFrequency::kSyncEvery5Frames, 
     eAttributeSyncFrequency::kSyncEvery10Frames, 
     eAttributeSyncFrequency::kSyncEvery30Frames};

constexpr  SyncLevelConfigs syncConfigsLevel2{
    eAttributeSyncFrequency::kSyncEvery1Frame,
    eAttributeSyncFrequency::kSyncEvery2Frames, 
    eAttributeSyncFrequency::kSyncEvery5Frames};


constexpr  SyncLevelConfigs syncConfigsLevel3{
    eAttributeSyncFrequency::kSyncEvery1Frame};

// 根据距离级别执行同步逻辑的函数
void SyncByDistanceLevel(const entt::entity& entity, EntityVector& entityList, int level, const double delta) {
    const auto frameCount = tlsGame.frameTime.frame_count();
    
    const SyncLevelConfigs* syncConfigs = nullptr;
    
    if (level == 1) {
        syncConfigs = &syncConfigsLevel1;
        ActorStateAttributeSyncUtil::GetNearLevel1EntityList(entity, entityList);
    }
    else if (level == 2) {
        syncConfigs = &syncConfigsLevel2;
        ActorStateAttributeSyncUtil::GetNearLevel2EntityList(entity, entityList);
    }
    else if (level == 3) {
        syncConfigs = &syncConfigsLevel3;
        ActorStateAttributeSyncUtil::GetNearLevel3EntityList(entity, entityList);
    }

    ActorStateAttributeSyncUtil::SyncBasicAttributes(entity, entityList, delta);  // 只同步基础属性

    if (nullptr == syncConfigs)
    {
        return;
    }
    
    // 根据配置和帧数执行同步
    for (const auto& config : *syncConfigs) {
        if (frameCount % config == 0) {
            ActorStateAttributeSyncUtil::SyncAttributes(entity, entityList, config, delta);
        }
    }

    entityList.clear();  // 清空列表为下一个距离级别做准备
}

void ActorStateAttributeSyncSystem::Update(const double delta)
{
    EntityVector entityList;

    // 遍历所有带有 Transform 组件的实体
    for (auto [entity, transform] : tls.registry.view<Transform>().each())
    {
        // 处理各距离级别的同步，传入距离级别参数
        SyncByDistanceLevel(entity, entityList, 1, delta);  // 近距离同步
        SyncByDistanceLevel(entity, entityList, 2, delta);  // 中距离同步
        SyncByDistanceLevel(entity, entityList, 3, delta);  // 远距离同步
    }
}
