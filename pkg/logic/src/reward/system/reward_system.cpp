#include "reward_system.h"

#include "muduo/base/Logging.h"
#include "reward/comp/reward_comp.h"
#include "thread_local/storage.h"

void RewardSystem::InitializeActorComponents(entt::entity entityId)
{
    tls.registry.emplace<RewardComp>(entityId);
}

// 领取奖励
void RewardSystem::ClaimReward(entt::entity entityId, uint32_t rewardIndex) {
    if (rewardIndex < 0 || rewardIndex >= kRewardMaxBitIndex) {
        LOG_ERROR << "奖励编号无效！";
        return;
    }

    auto& rewards = tls.registry.get<RewardComp>(entityId).rewards;

    if (rewards[rewardIndex]) {
        LOG_ERROR << "奖励 " << rewardIndex << " 已经领取过了！";
    }
    else {
        rewards.set(rewardIndex);  // 设置该奖励为已领取
        LOG_INFO << "恭喜你领取了奖励 " << rewardIndex << "！";
    }
}

// 检查某个奖励是否已经领取
bool RewardSystem::IsRewardClaimed(entt::entity entityId, uint32_t rewardIndex) {
    if (rewardIndex < 0 || rewardIndex >= kRewardMaxBitIndex) {
        LOG_ERROR << "奖励编号无效！";
        return false;
    }

    auto& rewards = tls.registry.get<RewardComp>(entityId).rewards;

    return rewards[rewardIndex];
}

// 显示所有奖励的领取状态
void RewardSystem::ShowRewardStatus() {
    
}