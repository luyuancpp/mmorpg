#include "reward_system.h"

#include "muduo/base/Logging.h"
#include "proto/table/common_error_tip.pb.h"
#include "proto/table/reward_error_tip.pb.h"
#include "reward/comp/reward_comp.h"

#include <threading/registry_manager.h>

void RewardSystem::InitializeActorComponents(entt::entity entityId)
{
    tlsRegistryManager.actorRegistry.emplace<RewardComp>(entityId);
}

uint32_t RewardSystem::ClaimRewardByRewardId(entt::entity entityId, uint32_t rewardId) {
    if (!RewardBitMap.contains(rewardId)) {
        LOG_ERROR << "ClaimRewardByRewardId failed: Invalid Reward ID. Entity ID: " 
                  << tlsRegistryManager.actorRegistry.get<Guid>(entityId) << ", Reward ID: " << rewardId;
        return kInvalidTableId;
    }

    const auto rewardIndex = RewardBitMap.at(rewardId);
    const auto result = ClaimRewardByIndex(entityId, rewardIndex);

    if (result == kSuccess) {
        LOG_INFO << "ClaimRewardByRewardId succeeded. Entity ID: " << tlsRegistryManager.actorRegistry.get<Guid>(entityId)
                 << ", Reward ID: " << rewardId << ", Reward Index: " << rewardIndex;
    }

    return result;
}


uint32_t RewardSystem::ClaimRewardByIndex(entt::entity entityId, uint32_t rewardIndex) {
    if (rewardIndex < 0 || rewardIndex >= kRewardMaxBitIndex) {
        LOG_ERROR << "ClaimRewardByIndex failed: Reward index out of range. Entity ID: " 
                  << tlsRegistryManager.actorRegistry.get<Guid>(entityId) << ", Reward Index: " << rewardIndex;
        return kIndexOutOfRange;
    }

    auto& rewards = tlsRegistryManager.actorRegistry.get<RewardComp>(entityId).rewards;

    if (rewards[rewardIndex]) {
        LOG_ERROR << "ClaimRewardByIndex failed: Reward already claimed. Entity ID: " 
                  << tlsRegistryManager.actorRegistry.get<Guid>(entityId) << ", Reward Index: " << rewardIndex;
        return kRewardAlreadyClaimed;
    }

    rewards.set(rewardIndex);  // 设置该奖励为已领取

    LOG_INFO << "Reward claimed successfully. Entity ID: " << tlsRegistryManager.actorRegistry.get<Guid>(entityId) 
             << ", Reward Index: " << rewardIndex;

    return kSuccess;
}

bool RewardSystem::IsRewardClaimedById(entt::entity entityId, uint32_t rewardId)
{
    if (!RewardBitMap.contains(rewardId)) {
        LOG_ERROR << "IsRewardClaimedById failed: Invalid Reward ID. Entity ID: "
            << tlsRegistryManager.actorRegistry.get<Guid>(entityId) << ", Reward ID: " << rewardId;
        return false;
    }

    auto rewardIndex = RewardBitMap.at(rewardId);

    return IsRewardClaimedByIndex(entityId, rewardId);
}

bool RewardSystem::IsRewardClaimedByIndex(entt::entity entityId, uint32_t rewardIndex) {
    if (rewardIndex < 0 || rewardIndex >= kRewardMaxBitIndex) {
        LOG_ERROR << "IsRewardClaimed failed: Reward index out of range. Entity ID: " 
                  << tlsRegistryManager.actorRegistry.get<Guid>(entityId) << ", Reward Index: " << rewardIndex;
        return false;
    }

    auto& rewards = tlsRegistryManager.actorRegistry.get<RewardComp>(entityId).rewards;
    bool claimed = rewards[rewardIndex];

    return claimed;
}

void RewardSystem::ShowRewardStatus() {
    LOG_INFO << "=== Displaying Reward Status for All Entities ===";

    // 获取所有拥有 RewardComp 的实体
    tlsRegistryManager.actorRegistry.view<RewardComp>().each(
        [](entt::entity entity, const RewardComp& rewardComp) {
            ShowEntityRewardStatus(entity, rewardComp);
        });

    LOG_INFO << "=== End of Reward Status Display ===";
}

// 显示单个实体的奖励状态
void RewardSystem::ShowEntityRewardStatus(entt::entity entityId, const RewardComp& rewardComp) {
    LOG_INFO << "Entity ID: " << tlsRegistryManager.actorRegistry.get<Guid>(entityId);

    // 遍历所有奖励
    for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
        const bool isClaimed = rewardComp.rewards[i];
        LOG_INFO << "  Reward " << i << ": " << (isClaimed ? "Claimed" : "Not Claimed");
    }
}

void RewardSystem::CountRewardStatistics() {
    uint32_t totalClaimed = 0;
    uint32_t totalRewards = 0;

    tlsRegistryManager.actorRegistry.view<RewardComp>().each([&](const RewardComp& rewardComp) {
        for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
            if (rewardComp.rewards[i]) {
                ++totalClaimed;
            }
            ++totalRewards;
        }
        });

    LOG_INFO << "Total Rewards: " << totalRewards
        << ", Claimed: " << totalClaimed
        << ", Unclaimed: " << (totalRewards - totalClaimed);
}

std::string RewardSystem::FormatRewardStatus(entt::entity entityId, const RewardComp& rewardComp) {
    std::ostringstream ss;
    ss << "Entity ID: " << tlsRegistryManager.actorRegistry.get<Guid>(entityId) << "\n";

    for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
        ss << "  Reward " << i << ": " << (rewardComp.rewards[i] ? "Claimed" : "Not Claimed") << "\n";
    }

    return ss.str();
}

bool RewardSystem::HasUnclaimedRewards(const RewardComp& rewardComp) {
    for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
        if (!rewardComp.rewards[i]) {
            return true;  // 如果存在未领取的奖励，返回 true
        }
    }
    return false;  // 所有奖励都已领取
}

bool RewardSystem::HasUnclaimedRewards(entt::entity entityId)
{
    auto& rewards = tlsRegistryManager.actorRegistry.get<RewardComp>(entityId);
    return HasUnclaimedRewards(rewards);
}

//例如仅显示未领取奖励的实体
void RewardSystem::ShowUnclaimedRewards() {
    LOG_INFO << "=== Displaying Entities with Unclaimed Rewards ===";

    tlsRegistryManager.actorRegistry.view<RewardComp>().each(
        [](auto entity, const RewardComp& rewardComp) {
            if (HasUnclaimedRewards(rewardComp)) {
                LOG_INFO << "Entity ID with unclaimed rewards: " << static_cast<uint32_t>(entity);
            }
        });

    LOG_INFO << "=== End of Unclaimed Rewards Display ===";
}

//统计未领取奖励的总数量
uint32_t RewardSystem::CountEntitiesWithUnclaimedRewards()  {
    uint32_t count = 0;

    tlsRegistryManager.actorRegistry.view<RewardComp>().each(
        [&count](const RewardComp& rewardComp) {
            if (HasUnclaimedRewards(rewardComp)) {
                ++count;
            }
        });

    return count;
}
