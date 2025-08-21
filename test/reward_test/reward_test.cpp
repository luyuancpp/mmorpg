#include "reward/system/reward_system.h"
#include <gtest/gtest.h>

#include "thread_local/storage.h"
#include "cpp_table_id_bit_index/reward_table_id_bit_index.h"
#include "reward/comp/reward_comp.h"
#include <thread_local/registry_manager.h>


decltype(auto) CreatePlayerEntityWithRewardComponent()
{
    const auto playerEntity = tlsRegistryManager.actorRegistry.create();
    RewardSystem::InitializeActorComponents(playerEntity);
    tlsRegistryManager.actorRegistry.emplace<Guid>(playerEntity);
    return playerEntity;
}

// 测试：初始化时所有奖励未领取
TEST(RewardSystemTest, InitializationTest) {

    auto playerEntity = CreatePlayerEntityWithRewardComponent();

    RewardSystem rewardSystem;

    // 检查所有奖励都未领取
    for (int i = 0; i < kRewardMaxBitIndex; ++i) {
        EXPECT_FALSE(rewardSystem.IsRewardClaimedByIndex(playerEntity, i));
    }
}

// 测试：领取一个奖励
TEST(RewardSystemTest, ClaimRewardTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

    RewardSystem rewardSystem;

    rewardSystem.ClaimRewardByIndex(playerEntity, 5);  // 领取奖励 5

    // 检查奖励 5 是否已领取
    EXPECT_TRUE(rewardSystem.IsRewardClaimedByIndex(playerEntity, 5));

    // 检查其他奖励仍未领取
    for (int i = 0; i < kRewardMaxBitIndex; ++i) {
        if (i != 5) {
            EXPECT_FALSE(rewardSystem.IsRewardClaimedByIndex(playerEntity, i));
        }
    }
}

// 测试：重复领取同一个奖励
TEST(RewardSystemTest, ClaimAlreadyClaimedRewardTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

    RewardSystem rewardSystem;

    rewardSystem.ClaimRewardByIndex(playerEntity, 3);  // 领取奖励 3
    rewardSystem.ClaimRewardByIndex(playerEntity, 3);  // 再次领取奖励 3

    // 检查奖励 3 是否已领取
    EXPECT_TRUE(rewardSystem.IsRewardClaimedByIndex(playerEntity, 3));
}

// 测试：无效奖励编号
TEST(RewardSystemTest, InvalidRewardIndexTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

    RewardSystem rewardSystem;

    // 传入无效奖励编号
    rewardSystem.ClaimRewardByIndex(playerEntity, -1);  // 无效编号
    rewardSystem.ClaimRewardByIndex(playerEntity, kRewardMaxBitIndex);  // 超过最大编号
}

// 测试：显示奖励状态
TEST(RewardSystemTest, ShowRewardStatusTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

    RewardSystem rewardSystem;

    rewardSystem.ClaimRewardByIndex(playerEntity, 0);  // 领取奖励 0
    rewardSystem.ClaimRewardByIndex(playerEntity, 4);  // 领取奖励 4

    // 测试状态
    EXPECT_TRUE(rewardSystem.IsRewardClaimedByIndex(playerEntity, 0));
    EXPECT_TRUE(rewardSystem.IsRewardClaimedByIndex(playerEntity, 4));
    EXPECT_FALSE(rewardSystem.IsRewardClaimedByIndex(playerEntity, 1));
}
