#include "modules/reward/system/reward.h"
#include <gtest/gtest.h>

#include "table/code/reward_table.h"
#include "table/code/bit_index/reward_table_id_bit_index.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/reward_error_tip.pb.h"
#include <threading/registry_manager.h>
#include <config.h>
#include <node_config_manager.h>

decltype(auto) CreatePlayerEntityWithRewardComponent(){
    const auto playerEntity = tlsRegistryManager.actorRegistry.create();
    tlsRegistryManager.actorRegistry.emplace<Guid>(playerEntity);

    return playerEntity;
}

// 测试：初始化时所有奖励未领取
TEST(RewardClaimSystemTest, InitializationTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

	auto& claimedRewards = tlsRegistryManager.actorRegistry.get_or_emplace<RewardComp>(playerEntity).rewards;

    RewardClaimSystem RewardClaimSystem;

    // 检查所有奖励都未领取
    for (int i = 0; i < kRewardMaxBitIndex; ++i) {
        EXPECT_FALSE(RewardClaimSystem.IsRewardClaimedByIndex(i, claimedRewards));
    }
}

// 测试：领取一个奖励
TEST(RewardClaimSystemTest, ClaimRewardTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

	auto& claimedRewards = tlsRegistryManager.actorRegistry.get_or_emplace<RewardComp>(playerEntity).rewards;

    RewardClaimSystem RewardClaimSystem;

    RewardClaimSystem.ClaimRewardByIndex(5, claimedRewards);  // 领取奖励 5

    // 检查奖励 5 是否已领取
    EXPECT_TRUE(RewardClaimSystem.IsRewardClaimedByIndex(5, claimedRewards));

    // 检查其他奖励仍未领取
    for (int i = 0; i < kRewardMaxBitIndex; ++i) {
        if (i != 5) {
            EXPECT_FALSE(RewardClaimSystem.IsRewardClaimedByIndex(i, claimedRewards));
        }
    }
}

// 测试：重复领取同一个奖励
TEST(RewardClaimSystemTest, ClaimAlreadyClaimedRewardTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

	auto& claimedRewards = tlsRegistryManager.actorRegistry.get_or_emplace<RewardComp>(playerEntity).rewards;

    RewardClaimSystem RewardClaimSystem;

    RewardClaimSystem.ClaimRewardByIndex(3, claimedRewards);  // 领取奖励 3
    RewardClaimSystem.ClaimRewardByIndex(3, claimedRewards);  // 再次领取奖励 3

    // 检查奖励 3 是否已领取
    EXPECT_TRUE(RewardClaimSystem.IsRewardClaimedByIndex(3, claimedRewards));
}

// 测试：无效奖励编号
TEST(RewardClaimSystemTest, InvalidRewardIndexTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

	auto& claimedRewards = tlsRegistryManager.actorRegistry.get_or_emplace<RewardComp>(playerEntity).rewards;

    RewardClaimSystem RewardClaimSystem;

    // 传入无效奖励编号
    EXPECT_EQ(kIndexOutOfRange, RewardClaimSystem.ClaimRewardByIndex(-1, claimedRewards));  // 无效编号
    EXPECT_EQ(kIndexOutOfRange, RewardClaimSystem.ClaimRewardByIndex(kRewardMaxBitIndex, claimedRewards));  // 超过最大编号
}

// 测试：显示奖励状态
TEST(RewardClaimSystemTest, ShowRewardStatusTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

	auto& claimedRewards = tlsRegistryManager.actorRegistry.get_or_emplace<RewardComp>(playerEntity).rewards;

    RewardClaimSystem RewardClaimSystem;

    RewardClaimSystem.ClaimRewardByIndex(0, claimedRewards);  // 领取奖励 0
    RewardClaimSystem.ClaimRewardByIndex(4, claimedRewards);  // 领取奖励 4

    // 测试状态
    EXPECT_TRUE(RewardClaimSystem.IsRewardClaimedByIndex(0, claimedRewards));
    EXPECT_TRUE(RewardClaimSystem.IsRewardClaimedByIndex(4, claimedRewards));
    EXPECT_FALSE(RewardClaimSystem.IsRewardClaimedByIndex(1, claimedRewards));
}


int main(int argc, char** argv)
{
	readBaseDeployConfig("etc/base_deploy_config.yaml", tlsNodeConfigManager.GetBaseDeployConfig());
	readGameConfig("etc/game_config.yaml", tlsNodeConfigManager.GetGameConfig());
	RewardTableManager::Instance().Load();
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
