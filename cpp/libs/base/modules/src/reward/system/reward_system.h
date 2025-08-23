#pragma once

#include <string>

#include "entt/src/entt/entity/registry.hpp"

struct RewardComp;

class RewardSystem {
public:
    static void InitializeActorComponents(entt::entity entityId);

    static uint32_t ClaimRewardByRewardId(entt::entity entityId, uint32_t rewardId);

    // 领取奖励
    static uint32_t ClaimRewardByIndex(entt::entity entityId, uint32_t rewardIndex);

    static bool IsRewardClaimedById(entt::entity  entityId, uint32_t rewardId);

    // 检查某个奖励是否已经领取
    static bool IsRewardClaimedByIndex(entt::entity  entityId, uint32_t rewardIndexs);

    static void ShowRewardStatus();

    static void ShowEntityRewardStatus(entt::entity entity, const RewardComp& rewardComp);

    static void CountRewardStatistics();

    static std::string FormatRewardStatus(entt::entity entity, const RewardComp& rewardComp);

    static bool HasUnclaimedRewards(entt::entity entityId);

    static void ShowUnclaimedRewards();

    static uint32_t CountEntitiesWithUnclaimedRewards();

private:
    static bool HasUnclaimedRewards(const RewardComp& rewardComp);

};

