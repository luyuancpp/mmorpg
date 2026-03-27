#include "modules/reward/system/reward.h"
#include <gtest/gtest.h>

#include "table/code/reward_table.h"
#include "table/code/bit_index/reward_table_id_bit_index.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/reward_error_tip.pb.h"
#include <thread_context/registry_manager.h>
#include "../test_config_helper.h"

decltype(auto) CreatePlayerEntityWithRewardComponent()
{
    const auto playerEntity = tlsEcs.actorRegistry.create();
    tlsEcs.actorRegistry.emplace<Guid>(playerEntity);

    return playerEntity;
}

// Test: all rewards unclaimed at initialization
TEST(RewardClaimSystemTest, InitializationTest)
{
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

    auto &claimedRewards = tlsEcs.actorRegistry.get_or_emplace<RewardComp>(playerEntity).claimedRewards;

    // verify all rewards are unclaimed
    for (int i = 0; i < kRewardMaxBitIndex; ++i)
    {
        EXPECT_FALSE(RewardClaimSystem::IsClaimedByIndex(i, claimedRewards));
    }
}

// Test: claim a single reward
TEST(RewardClaimSystemTest, ClaimRewardTest)
{
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

    auto &claimedRewards = tlsEcs.actorRegistry.get_or_emplace<RewardComp>(playerEntity).claimedRewards;

    RewardClaimSystem::ClaimByIndex(5, claimedRewards); // claim reward 5

    // verify reward 5 is claimed
    EXPECT_TRUE(RewardClaimSystem::IsClaimedByIndex(5, claimedRewards));

    // verify other rewards remain unclaimed
    for (int i = 0; i < kRewardMaxBitIndex; ++i)
    {
        if (i != 5)
        {
            EXPECT_FALSE(RewardClaimSystem::IsClaimedByIndex(i, claimedRewards));
        }
    }
}

// Test: claim the same reward twice
TEST(RewardClaimSystemTest, ClaimAlreadyClaimedRewardTest)
{
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

    auto &claimedRewards = tlsEcs.actorRegistry.get_or_emplace<RewardComp>(playerEntity).claimedRewards;

    RewardClaimSystem::ClaimByIndex(3, claimedRewards); // claim reward 3
    RewardClaimSystem::ClaimByIndex(3, claimedRewards); // claim reward 3 again

    // verify reward 3 is claimed
    EXPECT_TRUE(RewardClaimSystem::IsClaimedByIndex(3, claimedRewards));
}

// Test: invalid reward index
TEST(RewardClaimSystemTest, InvalidRewardIndexTest)
{
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

    auto &claimedRewards = tlsEcs.actorRegistry.get_or_emplace<RewardComp>(playerEntity).claimedRewards;

    // pass invalid reward index
    EXPECT_EQ(kIndexOutOfRange, RewardClaimSystem::ClaimByIndex(static_cast<uint32_t>(-1), claimedRewards));       // invalid index
    EXPECT_EQ(kIndexOutOfRange, RewardClaimSystem::ClaimByIndex(kRewardMaxBitIndex, claimedRewards)); // exceeds max index
}

// Test: display reward status
TEST(RewardClaimSystemTest, ShowRewardStatusTest)
{
    auto playerEntity = CreatePlayerEntityWithRewardComponent();

    auto &claimedRewards = tlsEcs.actorRegistry.get_or_emplace<RewardComp>(playerEntity).claimedRewards;

    RewardClaimSystem::ClaimByIndex(0, claimedRewards); // claim reward 0
    RewardClaimSystem::ClaimByIndex(4, claimedRewards); // claim reward 4

    // verify status
    EXPECT_TRUE(RewardClaimSystem::IsClaimedByIndex(0, claimedRewards));
    EXPECT_TRUE(RewardClaimSystem::IsClaimedByIndex(4, claimedRewards));
    EXPECT_FALSE(RewardClaimSystem::IsClaimedByIndex(1, claimedRewards));
}

int main(int argc, char **argv)
{
    if (!test_config::FindAndLoadTestConfig(argc, argv))
        return 1;
    RewardTableManager::Instance().Load();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
