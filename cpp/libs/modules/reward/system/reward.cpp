#include "reward.h"

#include "muduo/base/Logging.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/reward_error_tip.pb.h"
#include "reward/comp/reward.h"

#include <threading/registry_manager.h>

uint32_t RewardSystem::ClaimRewardByRewardId(uint32_t rewardId, RewardBitset& claimedRewards) {
    if (!RewardBitMap.contains(rewardId)) {
        return kInvalidTableId;
    }

    const auto rewardIndex = RewardBitMap.at(rewardId);
    const auto result = ClaimRewardByIndex(rewardIndex, claimedRewards);

    if (result == kSuccess) {
    }

    return result;
}


uint32_t RewardSystem::ClaimRewardByIndex(uint32_t rewardIndex, RewardBitset& claimedRewards) {
    if (rewardIndex < 0 || rewardIndex >= kRewardMaxBitIndex) {
        return kIndexOutOfRange;
    }

    if (claimedRewards[rewardIndex]) {
        return kRewardAlreadyClaimed;
    }

    claimedRewards.set(rewardIndex);  // 设置该奖励为已领取

    return kSuccess;
}

bool RewardSystem::IsRewardClaimedById(uint32_t rewardId, RewardBitset& claimedRewards)
{
    if (!RewardBitMap.contains(rewardId)) {
        return false;
    }

    auto rewardIndex = RewardBitMap.at(rewardId);

    return IsRewardClaimedByIndex(rewardId, claimedRewards);
}

bool RewardSystem::IsRewardClaimedByIndex(uint32_t rewardIndex, RewardBitset& claimedRewards) {
    if (rewardIndex < 0 || rewardIndex >= kRewardMaxBitIndex) {
        return false;
    }

    return claimedRewards[rewardIndex];
}

void RewardSystem::ShowRewardStatus() {
    LOG_INFO << "=== Displaying Reward Status for All Entities ===";

    // 获取所有拥有 RewardComp 的实体
    tlsRegistryManager.actorRegistry.view<RewardComp>().each(
        [](entt::entity entity, const RewardComp& rewardComp) {
            ShowEntityRewardStatus(rewardComp.rewards);
        });

    LOG_INFO << "=== End of Reward Status Display ===";
}

// 显示单个实体的奖励状态
void RewardSystem::ShowEntityRewardStatus(const RewardBitset& claimedRewards) {
    // 遍历所有奖励
    for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
        const bool isClaimed = claimedRewards[i];
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

std::string RewardSystem::FormatRewardStatus(entt::entity entityId, const RewardBitset& claimedRewards) {
    std::ostringstream ss;
    ss << "Entity ID: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(entityId) << "\n";

    for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
        ss << "  Reward " << i << ": " << (claimedRewards[i] ? "Claimed" : "Not Claimed") << "\n";
    }

    return ss.str();
}

bool RewardSystem::HasUnclaimedRewards(const RewardBitset& claimedRewards) {
    for (uint32_t i = 0; i < kRewardMaxBitIndex; ++i) {
        if (!claimedRewards[i]) {
            return true;  // 如果存在未领取的奖励，返回 true
        }
    }
    return false;  // 所有奖励都已领取
}


//例如仅显示未领取奖励的实体
void RewardSystem::ShowUnclaimedRewards() {
    LOG_INFO << "=== Displaying Entities with Unclaimed Rewards ===";

    tlsRegistryManager.actorRegistry.view<RewardComp>().each(
        [](auto entity, const RewardComp& rewardComp) {
            if (HasUnclaimedRewards(rewardComp.rewards)) {
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
            if (HasUnclaimedRewards(rewardComp.rewards)) {
                ++count;
            }
        });

    return count;
}
