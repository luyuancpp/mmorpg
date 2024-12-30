#include "reward_system.h"

#include "muduo/base/Logging.h"
#include "reward/comp/reward_comp.h"
#include "thread_local/storage.h"
#include "pbc/common_error_tip.pb.h"
#include "pbc/reward_error_tip.pb.h"

void RewardSystem::InitializeActorComponents(entt::entity entityId)
{
    tls.registry.emplace<RewardComp>(entityId);
}

uint32_t RewardSystem::ClaimRewardByRewardId(entt::entity entityId, uint32_t rewardId)
{
    if (!RewardBitMap.contains(rewardId))
    {
        return kInvalidTableId;
    }
    
    return ClaimRewardByIndex(entityId, RewardBitMap.at(rewardId));
}

// 领取奖励
uint32_t RewardSystem::ClaimRewardByIndex(entt::entity entityId, uint32_t rewardIndex) {
    if (rewardIndex < 0 || rewardIndex >= kRewardMaxBitIndex) {
        return kIndexOutOfRange;
    }

    auto& rewards = tls.registry.get<RewardComp>(entityId).rewards;

    if (rewards[rewardIndex]) {
        return kRewardAlreadyClaimed;
    }
    
    rewards.set(rewardIndex);  // 设置该奖励为已领取

    return kSuccess;
}

// 检查某个奖励是否已经领取
bool RewardSystem::IsRewardClaimed(entt::entity entityId, uint32_t rewardIndex) {
    if (rewardIndex < 0 || rewardIndex >= kRewardMaxBitIndex) {
        return false;
    }

    auto& rewards = tls.registry.get<RewardComp>(entityId).rewards;

    return rewards[rewardIndex];
}

// 显示所有奖励的领取状态
void RewardSystem::ShowRewardStatus() {
    
}