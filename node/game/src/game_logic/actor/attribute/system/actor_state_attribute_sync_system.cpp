#include "actor_state_attribute_sync_system.h"

#include "component/actor_comp.pb.h"
#include "game_logic/actor/attribute/constants/actor_state_attribute_constants.h"
#include "game_logic/actor/attribute/util/actor_state_attribute_sync_util.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"

// 定义帧同步频率的配置数组大小
constexpr uint32_t kSyncFrequencyArraySize = 5;

// 别名：定义同步频率数组类型
using SyncFrequencyArray = std::array<uint32_t, kSyncFrequencyArraySize>;

// 定义不同距离级别的同步频率
constexpr SyncFrequencyArray kLevel1SyncFrequencies{
    eAttributeSyncFrequency::kSyncEvery2Frames, 
    eAttributeSyncFrequency::kSyncEvery5Frames, 
    eAttributeSyncFrequency::kSyncEvery10Frames, 
    eAttributeSyncFrequency::kSyncEvery30Frames,
    eAttributeSyncFrequency::kSyncEvery60Frames};

constexpr SyncFrequencyArray kLevel2SyncFrequencies{
    eAttributeSyncFrequency::kSyncEvery2Frames, 
    eAttributeSyncFrequency::kSyncEvery5Frames,
    eAttributeSyncFrequency::kSyncEvery10Frames};

constexpr SyncFrequencyArray kLevel3SyncFrequencies{
    eAttributeSyncFrequency::kSyncEvery2Frames};

// 定义距离级别同步配置结构体，包含同步频率和获取实体列表的函数指针
struct DistanceSyncConfig {
    const SyncFrequencyArray& syncFrequencies;
    void (*retrieveEntityList)(const entt::entity, EntityVector&);
};

// 定义不同距离级别的同步配置
constexpr DistanceSyncConfig kDistanceSyncConfigs[] = {
    {kLevel1SyncFrequencies, ActorStateAttributeSyncUtil::GetNearbyLevel1Entities},
    {kLevel2SyncFrequencies, ActorStateAttributeSyncUtil::GetNearbyLevel2Entities},
    {kLevel3SyncFrequencies, ActorStateAttributeSyncUtil::GetNearbyLevel3Entities}
};

// 通用的同步函数，根据不同距离级别执行同步
void SyncAttributesForDistanceLevel(const entt::entity& entity, EntityVector& nearbyEntityList, const DistanceSyncConfig& distanceSyncConfig) {
    const auto currentFrame = tlsGame.frameTime.current_frame();

    // 获取该距离级别的实体列表
    distanceSyncConfig.retrieveEntityList(entity, nearbyEntityList);

    // 根据每个帧同步频率配置进行属性同步
    for (const auto& frequency : distanceSyncConfig.syncFrequencies) {
        // 添加检查，确保 frequency 大于 0
        if (frequency > 0 && currentFrame % frequency == 0) {
            ActorStateAttributeSyncUtil::SyncAttributes(entity, nearbyEntityList, frequency);
        }
    }

    // 清空实体列表，为下一个距离级别的同步做准备
    nearbyEntityList.clear();
}


// 系统更新函数，遍历每个实体并按距离级别进行属性同步
void ActorStateAttributeSyncSystem::Update(const double delta)
{
    EntityVector nearbyEntityList;

    for (auto [entity, transform] : tls.registry.view<Transform>().each())
    {
        // 始终同步基础属性
        ActorStateAttributeSyncUtil::SyncBasicAttributes(entity);

        // 处理各距离级别的同步，迭代 kDistanceSyncConfigs 数组，动态处理距离级别
        for (const auto& distanceSyncConfig : kDistanceSyncConfigs) {
            SyncAttributesForDistanceLevel(entity, nearbyEntityList, distanceSyncConfig);
        }
    }
}
