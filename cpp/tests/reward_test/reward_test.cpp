#include "modules/reward/system/reward.h"
#include <gtest/gtest.h>

#include "table/code/reward_table.h"
#include "table/code/bit_index/reward_table_id_bit_index.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/reward_error_tip.pb.h"
#include <thread_context/registry_manager.h>
#include <config.h>
#include <node_config_manager.h>

decltype(auto) CreatePlayerEntityWithRewardComponent(){
    const auto playerEntity = tlsRegistryManager.actorRegistry.create();
    tlsRegistryManager.actorRegistry.emplace<Guid>(playerEntity);

    return playerEntity;
}

// Test: all rewards unclaimed at initialization
TEST(RewardClaimSystemTest, InitializationTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

	auto& claimedRewards = tlsRegistryManager.actorRegistry.get_or_emplace<RewardComp>(playerEntity).GlobalRewards();

    RewardClaimSystem RewardClaimSystem;

    // verify all rewards are unclaimed
    for (int i = 0; i < kRewardMaxBitIndex; ++i) {
        EXPECT_FALSE(RewardClaimSystem.IsRewardClaimedByIndex(i, claimedRewards));
    }
}

// Test: claim a single reward
TEST(RewardClaimSystemTest, ClaimRewardTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

	auto& claimedRewards = tlsRegistryManager.actorRegistry.get_or_emplace<RewardComp>(playerEntity).GlobalRewards();

    RewardClaimSystem RewardClaimSystem;

    RewardClaimSystem.ClaimRewardByIndex(5, claimedRewards);  // claim reward 5

    // verify reward 5 is claimed
    EXPECT_TRUE(RewardClaimSystem.IsRewardClaimedByIndex(5, claimedRewards));

    // verify other rewards remain unclaimed
    for (int i = 0; i < kRewardMaxBitIndex; ++i) {
        if (i != 5) {
            EXPECT_FALSE(RewardClaimSystem.IsRewardClaimedByIndex(i, claimedRewards));
        }
    }
}

// Test: claim the same reward twice
TEST(RewardClaimSystemTest, ClaimAlreadyClaimedRewardTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

	auto& claimedRewards = tlsRegistryManager.actorRegistry.get_or_emplace<RewardComp>(playerEntity).GlobalRewards();

    RewardClaimSystem RewardClaimSystem;

    RewardClaimSystem.ClaimRewardByIndex(3, claimedRewards);  // claim reward 3
    RewardClaimSystem.ClaimRewardByIndex(3, claimedRewards);  // claim reward 3 again

    // verify reward 3 is claimed
    EXPECT_TRUE(RewardClaimSystem.IsRewardClaimedByIndex(3, claimedRewards));
}

// Test: invalid reward index
TEST(RewardClaimSystemTest, InvalidRewardIndexTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

	auto& claimedRewards = tlsRegistryManager.actorRegistry.get_or_emplace<RewardComp>(playerEntity).GlobalRewards();

    RewardClaimSystem RewardClaimSystem;

    // pass invalid reward index
    EXPECT_EQ(kIndexOutOfRange, RewardClaimSystem.ClaimRewardByIndex(-1, claimedRewards));  // invalid index
    EXPECT_EQ(kIndexOutOfRange, RewardClaimSystem.ClaimRewardByIndex(kRewardMaxBitIndex, claimedRewards));  // exceeds max index
}

// Test: display reward status
TEST(RewardClaimSystemTest, ShowRewardStatusTest) {
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

	auto& claimedRewards = tlsRegistryManager.actorRegistry.get_or_emplace<RewardComp>(playerEntity).GlobalRewards();

    RewardClaimSystem RewardClaimSystem;

    RewardClaimSystem.ClaimRewardByIndex(0, claimedRewards);  // claim reward 0
    RewardClaimSystem.ClaimRewardByIndex(4, claimedRewards);  // claim reward 4

    // verify status
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

