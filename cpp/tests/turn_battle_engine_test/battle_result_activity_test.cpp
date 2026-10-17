#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "services/battle/system/battle_result_activity.h"
#include "services/battle/system/result_commit.h"

// 活动局结果纯函数(battle_result_activity.h)的单元测试,docs/design/guild-phase2/06-activities.md §6.42。
// 覆盖:上下文回显 / 不回显、逃跑与阵亡名单升序去重、重发判定顺序、跨语言持久记录键契约。
// Redis / Kafka 编排在 battle_room_manager.cpp,不在本工程里测(与 settlement_outbox.h 同一拆法)。

using turnbattle::ActivityResultKey;
using turnbattle::ActivityResultRetryAction;
using turnbattle::ClassifyActivityResultRetry;
using turnbattle::FillBattleResultActivityFields;
using turnbattle::IsActivityBattle;

namespace
{
    ::BattleActivityContext MakeGuildTrialContext()
    {
        ::BattleActivityContext activity;
        activity.set_kind(::BATTLE_ACTIVITY_KIND_GUILD_TRIAL);
        activity.set_guild_id(7000000000001ULL);
        activity.set_activity_id(3);
        activity.set_period_key(20260929);
        activity.set_initiator_player_id(9216);
        activity.set_guild_period_key(20260929);
        return activity;
    }

    std::vector<uint64_t> FledOf(const contracts::kafka::BattleResultEvent &event)
    {
        return std::vector<uint64_t>(event.fled_player_ids().begin(), event.fled_player_ids().end());
    }

    std::vector<uint64_t> DeadOf(const contracts::kafka::BattleResultEvent &event)
    {
        return std::vector<uint64_t>(event.dead_player_ids().begin(), event.dead_player_ids().end());
    }
} // namespace

// kind == NONE:即使其它字段非 0 也不写上下文(普通对局),空名单写出来也是空。
TEST(BattleResultActivity, NoneKindLeavesContextUnset)
{
    ::BattleActivityContext activity;
    activity.set_guild_id(123); // kind 仍是 NONE:battle 只按 kind 判断,不看其余字段
    ASSERT_FALSE(IsActivityBattle(activity));

    contracts::kafka::BattleResultEvent event;
    event.set_battle_id(42);
    FillBattleResultActivityFields(activity, {}, {}, event);

    EXPECT_FALSE(event.has_activity_context());
    EXPECT_EQ(event.fled_player_ids_size(), 0);
    EXPECT_EQ(event.dead_player_ids_size(), 0);
    EXPECT_EQ(event.battle_id(), 42u); // 既有字段不被改动
}

// 同道历练:6 个字段逐一原样回显。
TEST(BattleResultActivity, GuildTrialContextEchoed)
{
    const auto activity = MakeGuildTrialContext();
    ASSERT_TRUE(IsActivityBattle(activity));

    contracts::kafka::BattleResultEvent event;
    FillBattleResultActivityFields(activity, {}, {}, event);

    ASSERT_TRUE(event.has_activity_context());
    const auto &echoed = event.activity_context();
    EXPECT_EQ(echoed.kind(), ::BATTLE_ACTIVITY_KIND_GUILD_TRIAL);
    EXPECT_EQ(echoed.guild_id(), 7000000000001ULL);
    EXPECT_EQ(echoed.activity_id(), 3u);
    EXPECT_EQ(echoed.period_key(), 20260929u);
    EXPECT_EQ(echoed.initiator_player_id(), 9216u);
    EXPECT_EQ(echoed.guild_period_key(), 20260929u);
}

// 名单升序去重;普通对局(kind NONE)也照样写名单。
TEST(BattleResultActivity, FledAndDeadSortedDeduplicated)
{
    contracts::kafka::BattleResultEvent event;
    FillBattleResultActivityFields(::BattleActivityContext{}, {9, 3, 9}, {5, 5}, event);

    EXPECT_FALSE(event.has_activity_context());
    EXPECT_EQ(FledOf(event), (std::vector<uint64_t>{3, 9}));
    EXPECT_EQ(DeadOf(event), (std::vector<uint64_t>{5}));
}

// U2(阵亡也得奖)只改 guild 的过滤口径:battle 仍如实回显阵亡名单,逃跑且阵亡的人两份名单都在。
TEST(BattleResultActivity, DeadListStillEchoedForGuildTrial)
{
    contracts::kafka::BattleResultEvent event;
    FillBattleResultActivityFields(MakeGuildTrialContext(), {11}, {12, 11}, event);

    ASSERT_TRUE(event.has_activity_context());
    EXPECT_EQ(FledOf(event), (std::vector<uint64_t>{11}));
    EXPECT_EQ(DeadOf(event), (std::vector<uint64_t>{11, 12}));
}

// proto3 开放枚举:未知 kind 值按活动对局处理(宁可多落一次库,不能把真实活动结果当普通对局)。
TEST(BattleResultActivity, UnknownKindTreatedAsActivity)
{
    auto activity = MakeGuildTrialContext();
    activity.set_kind(static_cast<::eBattleActivityKind>(7));
    ASSERT_TRUE(IsActivityBattle(activity));

    contracts::kafka::BattleResultEvent event;
    FillBattleResultActivityFields(activity, {}, {}, event);
    ASSERT_TRUE(event.has_activity_context());
    EXPECT_EQ(static_cast<int>(event.activity_context().kind()), 7);
}

// 判定顺序:已销账 > 次数用尽 > 重发。
TEST(BattleResultActivity, ClassifyRetryOrder)
{
    EXPECT_EQ(ClassifyActivityResultRetry(false, 99, 30), ActivityResultRetryAction::kDone);
    EXPECT_EQ(ClassifyActivityResultRetry(false, 30, 30), ActivityResultRetryAction::kDone);
    EXPECT_EQ(ClassifyActivityResultRetry(true, 30, 30), ActivityResultRetryAction::kExhausted);
    EXPECT_EQ(ClassifyActivityResultRetry(true, 29, 30), ActivityResultRetryAction::kResend);
    EXPECT_EQ(ClassifyActivityResultRetry(true, 0, 30), ActivityResultRetryAction::kResend);
}

// 跨语言键契约:guild(go/guild/internal/data/trial_result_record.go)按同一字面量读 / 删。
// 改这里的期望值 = 改契约,必须同改 Go 侧常量与其单测。
TEST(BattleResultActivity, ActivityResultKeyMatchesGuildContract)
{
    EXPECT_EQ(ActivityResultKey(123), "battle:activity_result:123");
    EXPECT_EQ(ActivityResultKey(std::numeric_limits<uint64_t>::max()),
              "battle:activity_result:18446744073709551615");
    EXPECT_EQ(turnbattle::kActivityResultTtlSec, 604800u);
    EXPECT_EQ(turnbattle::kActivityResultRetryIntervalSec, 10u);
    EXPECT_EQ(turnbattle::kActivityResultRetryMaxAttempts, 30u);
}

TEST(BattleResultCommit, UnknownCannotReleaseTerminalOrChangeOriginalBytes)
{
    turnbattle::ResultCommit commit("original-result-with-first-finished-at");
    const auto start = turnbattle::ResultCommit::Clock::time_point{};
    ASSERT_TRUE(commit.Begin(7, start));
    EXPECT_FALSE(commit.Confirmed());
    EXPECT_FALSE(commit.Begin(8, start));
    EXPECT_FALSE(commit.Deliver(99, true, start));
    EXPECT_FALSE(commit.Deliver(7, false, start));
    EXPECT_FALSE(commit.CanSend(start));
    EXPECT_TRUE(commit.CanSend(start + std::chrono::seconds(1)));
    ASSERT_TRUE(commit.Begin(8, start + std::chrono::seconds(1)));
    EXPECT_FALSE(commit.Deliver(7, true, start + std::chrono::seconds(1)));
    EXPECT_TRUE(commit.Deliver(8, true, start + std::chrono::seconds(1)));
    EXPECT_TRUE(commit.Confirmed());
    EXPECT_FALSE(commit.Deliver(8, true, start + std::chrono::seconds(2)));
    EXPECT_FALSE(commit.Begin(9, start + std::chrono::seconds(90)));
    EXPECT_EQ(commit.Payload(), "original-result-with-first-finished-at");
}

TEST(BattleResultCommit, FailedAdmissionUsesMonotonicBoundedBackoff)
{
    turnbattle::ResultCommit commit("same-bytes");
    auto current = turnbattle::ResultCommit::Clock::time_point{};
    const uint32_t delays[]{1,2,4,8,16,30,30};
    uint64_t correlation = 0;
    for (const auto seconds : delays)
    {
        ASSERT_TRUE(commit.Begin(++correlation, current));
        EXPECT_FALSE(commit.Deliver(correlation, false, current));
        EXPECT_FALSE(commit.CanSend(current + std::chrono::seconds(seconds) - std::chrono::milliseconds(1)));
        current += std::chrono::seconds(seconds);
        EXPECT_TRUE(commit.CanSend(current));
        EXPECT_FALSE(commit.Confirmed());
    }
}

TEST(BattleResultCommit, LiveRoomKeepsCreationReceiptBeyondDeadlineWhileResultIsUnknown)
{
    EXPECT_FALSE(turnbattle::CreationReceiptExpired(360000, 900000, true));
    EXPECT_TRUE(turnbattle::CreationReceiptExpired(360000, 900000, false));
    EXPECT_FALSE(turnbattle::CreationReceiptExpired(360000, 360000, false));
}

namespace
{
    contracts::kafka::BattleResultEvent MakeSettlementSourceResult()
    {
        contracts::kafka::BattleResultEvent event;
        event.set_battle_id(851);
        event.set_outcome(::BATTLE_OUTCOME_DRAW);
        event.set_finished_at_ms(123456);
        auto* team = event.add_teams();
        team->set_team_index(0);
        team->add_player_ids(92);
        team->add_player_ids(93);
        return event;
    }

    std::vector<::BattleSettlementData> MakeSettlementSources()
    {
        ::BattleSettlementData first;
        first.set_battle_id(851);
        first.set_player_id(92);
        first.set_health(27);
        first.set_mana(19);
        first.set_exp_gain(123);
        first.set_gold_gain(456);
        first.set_total_rounds(7);
        first.set_is_dead(true);
        first.set_fled(true);
        auto* consumed = first.add_items_consumed();
        consumed->set_item_table_id(1001);
        consumed->set_count(3);
        auto* gained = first.add_items_gained();
        gained->set_item_table_id(1002);
        gained->set_count(4);
        auto* pet = first.add_pets();
        pet->set_pet_id(923);
        pet->set_health(0);
        pet->set_mana(9);
        pet->set_is_dead(true);
        auto* defeated = first.add_defeated_monsters();
        defeated->set_monster_config_id(101);
        defeated->set_count(2);
        ::BattleSettlementData second;
        second.set_player_id(93);
        return {second, first};
    }
}

TEST(BattleResultCommit, FullSourcePreservesAllAssetsAndFixedBytesAfterUnknownAck)
{
    auto event = MakeSettlementSourceResult();
    auto source = MakeSettlementSources();
    auto expected = source.at(1);
    expected.set_outcome(::BATTLE_OUTCOME_DRAW);
    ASSERT_TRUE(turnbattle::FreezeBattleSettlementSource(event, source));
    ASSERT_EQ(event.settlements_size(), 2);
    EXPECT_EQ(event.settlement_format_version(), 1u);
    EXPECT_EQ(event.settlements(0).SerializeAsString(), expected.SerializeAsString());
    EXPECT_EQ(event.settlements(1).player_id(), 93u);
    EXPECT_EQ(event.settlements(1).battle_id(), 851u);
    EXPECT_EQ(event.settlements(1).outcome(), ::BATTLE_OUTCOME_DRAW);
    turnbattle::ResultCommit commit(event.SerializeAsString());
    source.at(1).set_gold_gain(99999);
    event.mutable_settlements(0)->clear_items_gained();
    const auto instant = turnbattle::ResultCommit::Clock::time_point{};
    ASSERT_TRUE(commit.Begin(1, instant));
    EXPECT_FALSE(commit.Deliver(1, false, instant));
    ASSERT_TRUE(commit.Begin(2, instant + std::chrono::seconds(1)));
    ASSERT_TRUE(commit.Deliver(2, true, instant + std::chrono::seconds(1)));
    contracts::kafka::BattleResultEvent restored;
    ASSERT_TRUE(restored.ParseFromString(commit.Payload()));
    EXPECT_EQ(restored.settlements(0).SerializeAsString(), expected.SerializeAsString());
    EXPECT_EQ(restored.finished_at_ms(), 123456u);
}

TEST(BattleResultCommit, InvalidSourceFailsBeforeChangingTheResult)
{
    const auto original = MakeSettlementSourceResult();
    const auto sources = MakeSettlementSources();
    auto event = original;
    auto invalid = sources;
    invalid.pop_back();
    EXPECT_FALSE(turnbattle::FreezeBattleSettlementSource(event, invalid));
    EXPECT_EQ(event.SerializeAsString(), original.SerializeAsString());
    invalid = sources;
    invalid.at(0).set_player_id(92);
    EXPECT_FALSE(turnbattle::FreezeBattleSettlementSource(event, invalid));
    invalid = sources;
    invalid.at(0).set_player_id(0);
    EXPECT_FALSE(turnbattle::FreezeBattleSettlementSource(event, invalid));
    invalid = sources;
    invalid.at(0).set_player_team_index(1);
    EXPECT_FALSE(turnbattle::FreezeBattleSettlementSource(event, invalid));
    invalid = sources;
    invalid.at(0).set_battle_id(852);
    EXPECT_FALSE(turnbattle::FreezeBattleSettlementSource(event, invalid));
    EXPECT_EQ(event.SerializeAsString(), original.SerializeAsString());
    ASSERT_TRUE(turnbattle::FreezeBattleSettlementSource(event, sources));
    const auto once = event.SerializeAsString();
    EXPECT_FALSE(turnbattle::FreezeBattleSettlementSource(event, sources));
    EXPECT_EQ(event.SerializeAsString(), once);
}

TEST(BattleResultCommit, OversizedCompleteSourceNeverPublishesATruncatedRecord)
{
    auto event = MakeSettlementSourceResult();
    event.GetReflection()->MutableUnknownFields(&event)->AddLengthDelimited(99,
        std::string(turnbattle::kMaxCompleteResultBytes, 'x'));
    const auto before = event.SerializeAsString();
    EXPECT_FALSE(turnbattle::FreezeBattleSettlementSource(event, MakeSettlementSources()));
    EXPECT_EQ(event.SerializeAsString(), before);
    EXPECT_EQ(event.settlement_format_version(), 0u);
}
