#include "actor_state_attribute_sync_system.h"

#include "component/actor_comp.pb.h"
#include "game_logic/actor/constants/actor_state_attribute_constants.h"
#include "game_logic/actor/util/actor_state_attribute_sync_util.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"

constexpr uint32_t kFrameArraySize = 5;

// 使用别名简化配置数组类型
using SyncLevelConfigs = std::array<uint32_t, kFrameArraySize>;

// 各级别的同步频率配置
constexpr SyncLevelConfigs syncConfigsLevel1{
    eAttributeSyncFrequency::kSyncEvery1Frame,
    eAttributeSyncFrequency::kSyncEvery2Frames, 
    eAttributeSyncFrequency::kSyncEvery5Frames, 
    eAttributeSyncFrequency::kSyncEvery10Frames, 
    eAttributeSyncFrequency::kSyncEvery30Frames};

constexpr SyncLevelConfigs syncConfigsLevel2{
    eAttributeSyncFrequency::kSyncEvery1Frame,
    eAttributeSyncFrequency::kSyncEvery2Frames, 
    eAttributeSyncFrequency::kSyncEvery5Frames};

constexpr SyncLevelConfigs syncConfigsLevel3{
    eAttributeSyncFrequency::kSyncEvery1Frame};

// 将距离级别、配置和实体列表获取函数绑定
struct SyncLevel {
    const SyncLevelConfigs& configs;
    std::function<void (const entt::entity&, const EntityVector&)> getEntityList;
};

// 不同距离级别的配置
const SyncLevel syncLevels[] = {
    {syncConfigsLevel1, ActorStateAttributeSyncUtil::GetNearLevel1EntityList},
    {syncConfigsLevel2, ActorStateAttributeSyncUtil::GetNearLevel2EntityList},
    {syncConfigsLevel3, ActorStateAttributeSyncUtil::GetNearLevel3EntityList}
};

// 执行同步的通用函数，处理所有距离级别
void SyncByDistanceLevel(const entt::entity& entity, EntityVector& entityList, const SyncLevel& syncLevel, const double delta) {
    const auto frameCount = tlsGame.frameTime.frame_count();

    // 获取对应距离级别的实体列表
    syncLevel.getEntityList(entity, entityList);

    // 始终同步基础属性
    ActorStateAttributeSyncUtil::SyncBasicAttributes(entity, entityList, delta);

    // 根据配置和帧数执行属性同步
    for (const auto& config : syncLevel.configs) {
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
        // 处理各距离级别的同步，迭代 syncLevels 数组，动态处理距离级别
        for (const auto& syncLevel : syncLevels) {
            SyncByDistanceLevel(entity, entityList, syncLevel, delta);
        }
    }
}
