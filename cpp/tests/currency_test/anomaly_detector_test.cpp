#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "modules/transaction_log/anomaly_detector.h"
#include "thread_context/ecs_context.h"

namespace {
class PlayerAnomalyWindowTest : public testing::Test
{
protected:
    static constexpr uint32_t kItemConfigId = 432123;

    void SetUp() override
    {
        AnomalyDetector::ClearAll();
        AnomalyThreshold threshold;
        threshold.maxCountPerWindow = std::numeric_limits<uint32_t>::max();
        threshold.maxAmountPerWindow = std::numeric_limits<uint32_t>::max();
        threshold.windowSeconds = 600;
        AnomalyDetector::SetCurrencyThreshold(kCurrencyGold, threshold);
        AnomalyDetector::SetItemThreshold(kItemConfigId, threshold);
    }

    void TearDown() override
    {
        for (const auto player : players)
        {
            if (tlsEcs.actorRegistry.valid(player)) tlsEcs.actorRegistry.destroy(player);
        }
        AnomalyDetector::ClearAll();
        AnomalyDetector::SetCurrencyThreshold(kCurrencyGold, AnomalyThreshold{});
        AnomalyDetector::SetItemThreshold(kItemConfigId, AnomalyThreshold{});
    }

    entt::entity CreatePlayer()
    {
        const auto player = tlsEcs.actorRegistry.create();
        players.push_back(player);
        return player;
    }

    std::vector<entt::entity> players;
};

TEST_F(PlayerAnomalyWindowTest, WindowLivesOnPlayerEntityAndDiesWithIt)
{
    const auto player = CreatePlayer();
    EXPECT_EQ(0u, AnomalyDetector::GetCurrencyGainCount(player, kCurrencyGold));
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<PlayerAnomalyWindowComp>(player));

    EXPECT_FALSE(AnomalyDetector::RecordCurrencyGain(player, kCurrencyGold, 1));
    EXPECT_FALSE(AnomalyDetector::RecordItemGain(player, kItemConfigId, 1));
    ASSERT_TRUE(tlsEcs.actorRegistry.all_of<PlayerAnomalyWindowComp>(player));
    EXPECT_EQ(1u, AnomalyDetector::GetCurrencyGainCount(player, kCurrencyGold));
    tlsEcs.actorRegistry.destroy(player);

    const auto reused = CreatePlayer();
    EXPECT_EQ(entt::entt_traits<entt::entity>::to_entity(player),
              entt::entt_traits<entt::entity>::to_entity(reused));
    EXPECT_NE(player, reused);
    EXPECT_EQ(0u, AnomalyDetector::GetCurrencyGainCount(reused, kCurrencyGold));
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<PlayerAnomalyWindowComp>(reused));
}

TEST_F(PlayerAnomalyWindowTest, NullOrStaleEntityIsNotRecorded)
{
    const auto stale = CreatePlayer();
    tlsEcs.actorRegistry.destroy(stale);
    const auto reused = CreatePlayer();

    for (const auto player : {entt::entity{entt::null}, stale})
    {
        EXPECT_FALSE(AnomalyDetector::RecordCurrencyGain(player, kCurrencyGold, 1));
        EXPECT_FALSE(AnomalyDetector::RecordItemGain(player, kItemConfigId, 1));
        EXPECT_EQ(0u, AnomalyDetector::GetCurrencyGainCount(player, kCurrencyGold));
    }
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<PlayerAnomalyWindowComp>(reused));

    EXPECT_FALSE(AnomalyDetector::RecordCurrencyGain(reused, kCurrencyGold, 1));
    AnomalyDetector::ClearPlayer(stale);
    EXPECT_EQ(1u, AnomalyDetector::GetCurrencyGainCount(reused, kCurrencyGold));
}

TEST_F(PlayerAnomalyWindowTest, ClearAllDropsAllWindowsWithoutDestroyingPlayers)
{
    const auto first = CreatePlayer();
    const auto second = CreatePlayer();
    EXPECT_FALSE(AnomalyDetector::RecordCurrencyGain(first, kCurrencyGold, 1));
    EXPECT_FALSE(AnomalyDetector::RecordItemGain(second, kItemConfigId, 1));
    ASSERT_TRUE(tlsEcs.actorRegistry.any_of<PlayerAnomalyWindowComp>(first));
    ASSERT_TRUE(tlsEcs.actorRegistry.any_of<PlayerAnomalyWindowComp>(second));

    AnomalyDetector::ClearAll();

    EXPECT_TRUE(tlsEcs.actorRegistry.valid(first));
    EXPECT_TRUE(tlsEcs.actorRegistry.valid(second));
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<PlayerAnomalyWindowComp>(first));
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<PlayerAnomalyWindowComp>(second));
    EXPECT_EQ(0u, AnomalyDetector::GetCurrencyGainCount(first, kCurrencyGold));
}

TEST_F(PlayerAnomalyWindowTest, DisabledRecordingAndClearingDoNotCreateWindows)
{
    const auto player = CreatePlayer();
    AnomalyThreshold disabled;
    disabled.maxCountPerWindow = 0;
    disabled.maxAmountPerWindow = 0;
    AnomalyDetector::SetCurrencyThreshold(kCurrencyGold, disabled);
    AnomalyDetector::SetItemThreshold(kItemConfigId, disabled);

    EXPECT_FALSE(AnomalyDetector::RecordCurrencyGain(player, kCurrencyGold, 1));
    EXPECT_FALSE(AnomalyDetector::RecordItemGain(player, kItemConfigId, 1));
    AnomalyDetector::ClearPlayer(player);
    EXPECT_EQ(0u, AnomalyDetector::GetCurrencyGainCount(player, kCurrencyGold));
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<PlayerAnomalyWindowComp>(player));
}
} // namespace
