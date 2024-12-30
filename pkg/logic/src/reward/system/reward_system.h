#pragma once

#include "entt/src/entt/entity/registry.hpp"

class RewardSystem {
public:
    static void InitializeActorComponents(entt::entity entityId);

    static uint32_t ClaimRewardByRewardId(entt::entity entityId, uint32_t rewardId);

    // 领取奖励
    static uint32_t ClaimRewardByIndex(entt::entity entityId, uint32_t rewardIndex);

    // 检查某个奖励是否已经领取
    static bool IsRewardClaimed(entt::entity  entityId, uint32_t rewardIndexs);

    static void ShowRewardStatus();

private:
};

