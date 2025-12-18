#pragma once

#include <string>

#include "entt/src/entt/entity/registry.hpp"

#include "modules/reward/comp/reward.h"

struct RewardComp;

class RewardSystem {
public:
    static uint32_t ClaimRewardByRewardId( uint32_t rewardId, RewardBitset& claimedRewards);

    // 领取奖励
    static uint32_t ClaimRewardByIndex( uint32_t rewardIndex, RewardBitset& claimedRewards);

    static bool IsRewardClaimedById( uint32_t rewardId, RewardBitset& claimedRewards);

    // 检查某个奖励是否已经领取
    static bool IsRewardClaimedByIndex( uint32_t rewardIndexs, RewardBitset& claimedRewards);

    static void ShowRewardStatus();

    static void ShowEntityRewardStatus( const RewardBitset& claimedRewards);

    static void CountRewardStatistics();

    static std::string FormatRewardStatus(entt::entity entity, const RewardBitset& claimedRewards);

    static bool HasUnclaimedRewards(const RewardBitset& claimedRewards);

    static void ShowUnclaimedRewards();

    static uint32_t CountEntitiesWithUnclaimedRewards();


};

