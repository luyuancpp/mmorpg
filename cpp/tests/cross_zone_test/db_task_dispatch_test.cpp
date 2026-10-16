#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "thread_context/ecs_context.h"
#include "engine/core/type_define/player_guid_comp.h"
#include "engine/core/type_define/type_define.h"
#include "engine/infra/messaging/kafka/kafka_producer.h"
#include "modules/bag/comp/player_bags_comp.h"
#include "modules/currency/comp/player_currency_comp.h"
#include "modules/mission/comp/mission_comp.h"
#include "player/comp/last_persisted_snapshot_comp.h"
#include "player/system/db_task_dispatch_system.h"
#include "player/system/player_data_loader.h"
#include "player/system/player_lifecycle.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/asset_op_ledger_comp.pb.h"
#include "proto/common/component/battle_settlement_ledger_comp.pb.h"
#include "proto/common/component/player_inbox_ledger_comp.pb.h"
#include "proto/common/component/currency_comp.pb.h"
#include "proto/common/component/player_attribute_comp.pb.h"
#include "proto/common/component/player_comp.pb.h"         // UnregisterPlayer
#include "proto/common/component/player_network_comp.pb.h" // PlayerSessionSnapshotComp
#include "proto/common/component/player_pet_comp.pb.h"
#include "proto/common/component/player_skill_comp.pb.h"
#include "services/scene/player/system/player_exit_intent.h"
#include "type_alias/player_session_type_alias.h" // SessionMap

// ---------------------------------------------------------------------------
// db_task_dispatch_test —— scene 发 DBTask 失败后的脏快照修复(S5a)。
//
// 三层:
//   DbTaskDispatchLedger  纯账本:代号比较下的 NeedsResave、票据结局、超时兜底、条目回收;
//   ExitDbTaskGate        纯决策:下线收尾的回执闸门与 rekick 判据;
//   DbTaskDispatchEcs     直接驱动 PlayerLifecycleSystem 的收尾口 + DbTaskDispatchSystem::OnDeliveryReport / Pump。
//                         宿主里没有 Redis 客户端(GetPlayerDataRedis() 为空):用例都不走到真重存
//                         (要么快路径不写盘、要么拒存在 Redis 之前返回),HasUnsettledPlayerSave 如实为 false;
//                         本线程的 KafkaProducer 没登记 broker,真发送一律被同步拒绝。
// 固件每个用例前后都清账本,保证不污染 cross_zone_test.cpp 里依赖空账本的 ExitPersist* 用例
// (空账本 = 闸门恒判 kFinish = 与改动前相同的收尾行为)。
// ---------------------------------------------------------------------------

namespace
{
using db_task_dispatch::Clock;
using db_task_dispatch::DeliveryEffect;
using db_task_dispatch::Ledger;
using db_task_dispatch_stats::Counter;

constexpr db_task_dispatch::PlayerId kLedgerPlayer = 41;
constexpr db_task_dispatch::PlayerId kLedgerOtherPlayer = 42;

// 用例里的「现在」:固定起点加偏移,不读墙钟。
Clock::time_point At(std::chrono::seconds offset)
{
    return Clock::time_point{} + std::chrono::hours(1) + offset;
}
} // namespace

// ============================================================================
// 1. 纯账本
// ============================================================================

TEST(DbTaskDispatchLedger, BothTicketsDeliveredSettles)
{
    Ledger ledger;
    const auto gen = ledger.BeginSave(kLedgerPlayer);
    const auto first = ledger.IssueTicket(kLedgerPlayer, gen, At(std::chrono::seconds(0)));
    const auto second = ledger.IssueTicket(kLedgerPlayer, gen, At(std::chrono::seconds(0)));
    ledger.FinishSave(kLedgerPlayer, gen, /*allQueued=*/true);
    EXPECT_NE(first, db_task_dispatch::kNoTicket);
    EXPECT_NE(first, second);
    EXPECT_EQ(ledger.InFlight(kLedgerPlayer), 2u);
    EXPECT_FALSE(ledger.IsSettled(kLedgerPlayer)) << "还有在途票据";

    const auto one = ledger.OnDelivery(first, /*delivered=*/true);
    EXPECT_EQ(one.effect, DeliveryEffect::kDelivered);
    EXPECT_EQ(one.playerId, kLedgerPlayer);
    EXPECT_EQ(one.inFlightAfter, 1u);

    const auto two = ledger.OnDelivery(second, /*delivered=*/true);
    EXPECT_EQ(two.effect, DeliveryEffect::kDelivered);
    EXPECT_EQ(two.inFlightAfter, 0u);
    EXPECT_FALSE(two.needsResaveAfter);
    EXPECT_TRUE(ledger.IsSettled(kLedgerPlayer));
    EXPECT_FALSE(ledger.NeedsResave(kLedgerPlayer));
    EXPECT_EQ(ledger.OutstandingTickets(), 0u);
}

TEST(DbTaskDispatchLedger, SyncRejectionNeedsResaveUntilNextFullyQueuedSave)
{
    Ledger ledger;
    const auto gen1 = ledger.BeginSave(kLedgerPlayer);
    const auto rejected = ledger.IssueTicket(kLedgerPlayer, gen1, At(std::chrono::seconds(0)));
    const auto queued = ledger.IssueTicket(kLedgerPlayer, gen1, At(std::chrono::seconds(0)));
    ledger.OnProduceRejected(rejected);
    ledger.FinishSave(kLedgerPlayer, gen1, /*allQueued=*/false);
    EXPECT_TRUE(ledger.NeedsResave(kLedgerPlayer)) << "两张子表只送出一张:MySQL 欠一次整份写";
    EXPECT_EQ(ledger.InFlight(kLedgerPlayer), 1u) << "被拒的那张不算在途,永远不会有回执";

    const auto gen2 = ledger.BeginSave(kLedgerPlayer);
    EXPECT_GT(gen2, gen1);
    ledger.IssueTicket(kLedgerPlayer, gen2, At(std::chrono::seconds(1)));
    ledger.IssueTicket(kLedgerPlayer, gen2, At(std::chrono::seconds(1)));
    ledger.FinishSave(kLedgerPlayer, gen2, /*allQueued=*/true);
    EXPECT_FALSE(ledger.NeedsResave(kLedgerPlayer)) << "之后一次整份入队的存盘盖过了那次失败";

    EXPECT_EQ(ledger.OnDelivery(queued, /*delivered=*/true).effect, DeliveryEffect::kDelivered);
    EXPECT_FALSE(ledger.NeedsResave(kLedgerPlayer));
}

TEST(DbTaskDispatchLedger, DeliveryFailureOfLatestSaveNeedsResave)
{
    Ledger ledger;
    const auto gen = ledger.BeginSave(kLedgerPlayer);
    const auto failed = ledger.IssueTicket(kLedgerPlayer, gen, At(std::chrono::seconds(0)));
    ledger.IssueTicket(kLedgerPlayer, gen, At(std::chrono::seconds(0)));
    ledger.FinishSave(kLedgerPlayer, gen, /*allQueued=*/true);

    const auto outcome = ledger.OnDelivery(failed, /*delivered=*/false);
    EXPECT_EQ(outcome.effect, DeliveryEffect::kFailedNeedsResave);
    EXPECT_TRUE(outcome.needsResaveAfter);
    EXPECT_EQ(outcome.inFlightAfter, 1u);
    EXPECT_TRUE(ledger.NeedsResave(kLedgerPlayer));
}

TEST(DbTaskDispatchLedger, FailureSupersededByLaterFullyQueuedSave)
{
    Ledger ledger;
    const auto gen1 = ledger.BeginSave(kLedgerPlayer);
    const auto old = ledger.IssueTicket(kLedgerPlayer, gen1, At(std::chrono::seconds(0)));
    ledger.FinishSave(kLedgerPlayer, gen1, /*allQueued=*/true);
    const auto gen2 = ledger.BeginSave(kLedgerPlayer);
    ledger.IssueTicket(kLedgerPlayer, gen2, At(std::chrono::seconds(1)));
    ledger.FinishSave(kLedgerPlayer, gen2, /*allQueued=*/true);

    const auto outcome = ledger.OnDelivery(old, /*delivered=*/false);
    EXPECT_EQ(outcome.effect, DeliveryEffect::kFailedSuperseded) << "更新的整份存盘已入队,旧失败不需要重存";
    EXPECT_FALSE(outcome.needsResaveAfter);
    EXPECT_FALSE(ledger.NeedsResave(kLedgerPlayer));
}

TEST(DbTaskDispatchLedger, FailureBeforeFinishSaveIsNotCleared)
{
    // 回执可能在 send() 内部的 poll 里就到,早于本次存盘的 FinishSave:代号比较让结论与到达顺序无关。
    Ledger ledger;
    const auto gen1 = ledger.BeginSave(kLedgerPlayer);
    const auto earlier = ledger.IssueTicket(kLedgerPlayer, gen1, At(std::chrono::seconds(0)));
    ledger.FinishSave(kLedgerPlayer, gen1, /*allQueued=*/true);
    ASSERT_EQ(ledger.OnDelivery(earlier, /*delivered=*/true).effect, DeliveryEffect::kDelivered);

    const auto gen2 = ledger.BeginSave(kLedgerPlayer);
    const auto current = ledger.IssueTicket(kLedgerPlayer, gen2, At(std::chrono::seconds(1)));
    EXPECT_EQ(ledger.OnDelivery(current, /*delivered=*/false).effect, DeliveryEffect::kFailedNeedsResave);
    ledger.FinishSave(kLedgerPlayer, gen2, /*allQueued=*/true); // 这次存盘自己的票据已经失败了
    EXPECT_TRUE(ledger.NeedsResave(kLedgerPlayer)) << "FinishSave 晚到不能抹掉同一代的失败";
}

TEST(DbTaskDispatchLedger, LocalFailureNeedsResave)
{
    Ledger ledger;
    const auto gen = ledger.BeginSave(kLedgerPlayer);
    ledger.MarkLocalFailure(kLedgerPlayer, gen);
    ledger.FinishSave(kLedgerPlayer, gen, /*allQueued=*/false);
    EXPECT_TRUE(ledger.NeedsResave(kLedgerPlayer));
    EXPECT_EQ(ledger.InFlight(kLedgerPlayer), 0u);
    EXPECT_FALSE(ledger.IsSettled(kLedgerPlayer));
}

TEST(DbTaskDispatchLedger, DuplicateOrUnknownReportIsIgnored)
{
    Ledger ledger;
    const auto gen = ledger.BeginSave(kLedgerPlayer);
    const auto ticket = ledger.IssueTicket(kLedgerPlayer, gen, At(std::chrono::seconds(0)));
    ledger.FinishSave(kLedgerPlayer, gen, /*allQueued=*/true);
    ASSERT_EQ(ledger.OnDelivery(ticket, /*delivered=*/true).effect, DeliveryEffect::kDelivered);

    EXPECT_EQ(ledger.OnDelivery(ticket, /*delivered=*/false).effect, DeliveryEffect::kUnknownTicket)
        << "同一张票据的重复回执不改账";
    EXPECT_EQ(ledger.OnDelivery(999999, /*delivered=*/false).effect, DeliveryEffect::kUnknownTicket);
    EXPECT_EQ(ledger.InFlight(kLedgerPlayer), 0u) << "不下溢";
    EXPECT_FALSE(ledger.NeedsResave(kLedgerPlayer));

    ASSERT_TRUE(ledger.ForgetIfNoInFlight(kLedgerPlayer).forgotten);
    EXPECT_EQ(ledger.OnDelivery(ticket, /*delivered=*/false).effect, DeliveryEffect::kUnknownTicket)
        << "条目回收之后到达的回执同样忽略";
    EXPECT_EQ(ledger.TrackedPlayers(), 0u) << "迟到的回执不会把条目建回来";
    EXPECT_EQ(ledger.InFlight(kLedgerPlayer), 0u);
}

TEST(DbTaskDispatchLedger, ForgetOnlyWhenNoInFlight)
{
    Ledger ledger;
    EXPECT_FALSE(ledger.ForgetIfNoInFlight(kLedgerPlayer).forgotten) << "没有条目";

    const auto gen = ledger.BeginSave(kLedgerPlayer);
    const auto ticket = ledger.IssueTicket(kLedgerPlayer, gen, At(std::chrono::seconds(0)));
    ledger.FinishSave(kLedgerPlayer, gen, /*allQueued=*/true);
    const auto whileInFlight = ledger.ForgetIfNoInFlight(kLedgerPlayer);
    EXPECT_FALSE(whileInFlight.forgotten) << "还有在途票据,等回执";
    EXPECT_FALSE(whileInFlight.hadUnresolvedFailure);
    EXPECT_EQ(ledger.TrackedPlayers(), 1u);

    ledger.OnDelivery(ticket, /*delivered=*/false);
    const auto afterFailure = ledger.ForgetIfNoInFlight(kLedgerPlayer);
    EXPECT_TRUE(afterFailure.forgotten);
    EXPECT_TRUE(afterFailure.hadUnresolvedFailure) << "如实报告离开时仍欠 MySQL 一次写";
    EXPECT_EQ(ledger.TrackedPlayers(), 0u);

    const auto otherGen = ledger.BeginSave(kLedgerOtherPlayer);
    const auto otherTicket = ledger.IssueTicket(kLedgerOtherPlayer, otherGen, At(std::chrono::seconds(0)));
    ledger.FinishSave(kLedgerOtherPlayer, otherGen, /*allQueued=*/true);
    ledger.OnDelivery(otherTicket, /*delivered=*/true);
    const auto clean = ledger.ForgetIfNoInFlight(kLedgerOtherPlayer);
    EXPECT_TRUE(clean.forgotten);
    EXPECT_FALSE(clean.hadUnresolvedFailure);
}

TEST(DbTaskDispatchLedger, ExpireStaleTreatsTimedOutTicketsAsFailedInIssueOrder)
{
    constexpr auto kTimeout = std::chrono::seconds(330);
    Ledger ledger;
    const auto gen = ledger.BeginSave(kLedgerPlayer);
    const auto otherGen = ledger.BeginSave(kLedgerOtherPlayer);
    const auto oldest = ledger.IssueTicket(kLedgerPlayer, gen, At(std::chrono::seconds(0)));
    const auto settled = ledger.IssueTicket(kLedgerPlayer, gen, At(std::chrono::seconds(1)));
    ledger.IssueTicket(kLedgerOtherPlayer, otherGen, At(std::chrono::seconds(2)));
    const auto young = ledger.IssueTicket(kLedgerPlayer, gen, At(std::chrono::seconds(400)));
    ledger.FinishSave(kLedgerPlayer, gen, /*allQueued=*/true);
    ledger.FinishSave(kLedgerOtherPlayer, otherGen, /*allQueued=*/true);
    ASSERT_EQ(ledger.OnDelivery(settled, /*delivered=*/true).effect, DeliveryEffect::kDelivered);

    const auto expired = ledger.ExpireStale(At(std::chrono::seconds(332)), kTimeout);
    ASSERT_EQ(expired.size(), 2u) << "已有结局的跳过;碰到第一张没过期的就停";
    EXPECT_EQ(expired[0].playerId, kLedgerPlayer) << "按发票顺序";
    EXPECT_EQ(expired[0].effect, DeliveryEffect::kFailedNeedsResave);
    EXPECT_EQ(expired[1].playerId, kLedgerOtherPlayer) << "恰好满 timeout 也算到期";
    EXPECT_EQ(expired[1].effect, DeliveryEffect::kFailedNeedsResave);
    EXPECT_TRUE(ledger.NeedsResave(kLedgerPlayer));
    EXPECT_EQ(ledger.InFlight(kLedgerPlayer), 1u) << "young 还在途";

    EXPECT_TRUE(ledger.ExpireStale(At(std::chrono::seconds(332)), kTimeout).empty()) << "同一时刻再扫不重复";
    EXPECT_EQ(ledger.OnDelivery(oldest, /*delivered=*/true).effect, DeliveryEffect::kUnknownTicket)
        << "超时之后迟到的真回执不改账";
    EXPECT_EQ(ledger.OnDelivery(young, /*delivered=*/true).effect, DeliveryEffect::kDelivered);
}

TEST(DbTaskDispatchLedger, UnsettledPlayersSortedAndBounded)
{
    Ledger ledger;
    for (const db_task_dispatch::PlayerId playerId : {30u, 10u, 20u})
    {
        const auto gen = ledger.BeginSave(playerId);
        ledger.MarkLocalFailure(playerId, gen);
        ledger.FinishSave(playerId, gen, /*allQueued=*/false);
    }
    const auto settledGen = ledger.BeginSave(40);
    ledger.FinishSave(40, settledGen, /*allQueued=*/true);

    EXPECT_EQ(ledger.UnsettledPlayerCount(), 3u);
    EXPECT_EQ(ledger.UnsettledPlayers(2), (std::vector<db_task_dispatch::PlayerId>{10, 20})) << "升序后截断";
    EXPECT_EQ(ledger.UnsettledPlayers(10), (std::vector<db_task_dispatch::PlayerId>{10, 20, 30}));
    EXPECT_TRUE(ledger.UnsettledPlayers(0).empty());
}

TEST(DbTaskDispatchLedger, AllowsDirtySaveSkipTable)
{
    for (const bool allow : {false, true})
    {
        for (const bool matches : {false, true})
        {
            for (const bool needsResave : {false, true})
            {
                const bool expected = allow && matches && !needsResave;
                EXPECT_EQ(db_task_dispatch::AllowsDirtySaveSkip(allow, matches, needsResave), expected)
                    << "allow=" << allow << " matches=" << matches << " needs_resave=" << needsResave;
            }
        }
    }
}

TEST(DbTaskDispatchLedger, EffectNamesCoverEveryValue)
{
    for (std::size_t i = 0; i < db_task_dispatch::kDeliveryEffectCount; ++i)
    {
        EXPECT_STRNE(db_task_dispatch::DeliveryEffectName(static_cast<DeliveryEffect>(i)), "?");
    }
    EXPECT_STREQ(db_task_dispatch::DeliveryEffectName(DeliveryEffect::kCount), "?");
}

TEST(DbTaskDispatchStats, AlarmingIncreaseIgnoresSupersededFailures)
{
    using db_task_dispatch_stats::Snapshot;
    const auto index = [](Counter counter) { return static_cast<std::size_t>(counter); };

    Snapshot prev;
    Snapshot superseded = prev;
    superseded.values[index(Counter::kDeliveryFailed)] = 1;
    superseded.values[index(Counter::kDeliveryFailedSuperseded)] = 1;
    superseded.values[index(Counter::kExitAwaitAck)] = 5;
    superseded.values[index(Counter::kForcedResave)] = 2;
    EXPECT_FALSE(superseded.AlarmingIncreaseSince(prev)) << "被盖过的失败、等回执、强制重存都不是告警";

    Snapshot unsuperseded = superseded;
    unsuperseded.values[index(Counter::kDeliveryFailed)] = 2;
    EXPECT_TRUE(unsuperseded.AlarmingIncreaseSince(superseded));

    for (const Counter counter : {Counter::kProduceRejected, Counter::kLocalFailure, Counter::kTicketExpired,
                                  Counter::kExitResaveCapped, Counter::kExitUnsynced, Counter::kOrphanUnsynced})
    {
        Snapshot grown = prev;
        grown.values[index(counter)] = 1;
        EXPECT_TRUE(grown.AlarmingIncreaseSince(prev)) << db_task_dispatch_stats::kCounterNames[index(counter)];
    }
}

// ============================================================================
// 2. 下线收尾的回执闸门(纯决策)
// ============================================================================

TEST(ExitDbTaskGate, DecisionTable)
{
    using D = player_exit::DbTaskGateDecision;
    constexpr uint8_t kMax = player_exit::kMaxExitDbTaskResaveRounds;
    static_assert(kMax == 3, "用例按上限 3 写的;改上限时同步核对下面的表");
    EXPECT_EQ(player_exit::DecideDbTaskGate(0, false, 0, kMax), D::kFinish);
    EXPECT_EQ(player_exit::DecideDbTaskGate(2, false, 0, kMax), D::kAwaitAck);
    EXPECT_EQ(player_exit::DecideDbTaskGate(0, true, 0, kMax), D::kResave);
    EXPECT_EQ(player_exit::DecideDbTaskGate(1, true, 2, kMax), D::kResave) << "确定的失败优先于等剩下的回执";
    EXPECT_EQ(player_exit::DecideDbTaskGate(0, true, 3, kMax), D::kRetainExhausted);
    EXPECT_EQ(player_exit::DecideDbTaskGate(5, true, 3, kMax), D::kRetainExhausted);
    EXPECT_EQ(player_exit::DecideDbTaskGate(0, false, 3, kMax), D::kFinish) << "超限之后只要落定了照样收尾";
}

TEST(ExitDbTaskGate, RekickOnlyWhenRetainedExhaustedAndQuiet)
{
    constexpr uint8_t kMax = player_exit::kMaxExitDbTaskResaveRounds;
    EXPECT_TRUE(player_exit::ShouldRekickDbTaskExhausted(true, kMax, kMax, 0, true, false));
    EXPECT_FALSE(player_exit::ShouldRekickDbTaskExhausted(false, kMax, kMax, 0, true, false)) << "没停在闸门上";
    EXPECT_FALSE(player_exit::ShouldRekickDbTaskExhausted(true, static_cast<uint8_t>(kMax - 1), kMax, 0, true, false))
        << "没到上限 = 闸门自己的重存还在途";
    EXPECT_FALSE(player_exit::ShouldRekickDbTaskExhausted(true, kMax, kMax, 1, true, false)) << "有在途 DBTask 就等回执";
    EXPECT_FALSE(player_exit::ShouldRekickDbTaskExhausted(true, kMax, kMax, 0, false, false)) << "不欠 MySQL";
    EXPECT_FALSE(player_exit::ShouldRekickDbTaskExhausted(true, kMax, kMax, 0, true, true))
        << "有未落地的 Redis 存盘就等它落地";
}

TEST(ExitDbTaskGate, NamesCoverEveryValue)
{
    for (std::size_t i = 0; i < player_exit::kDbTaskGateDecisionCount; ++i)
    {
        EXPECT_STRNE(player_exit::DbTaskGateDecisionName(static_cast<player_exit::DbTaskGateDecision>(i)), "?");
    }
    EXPECT_STREQ(player_exit::DbTaskGateDecisionName(player_exit::DbTaskGateDecision::kCount), "?");
}

// ============================================================================
// 3. ECS 级:收尾口 + 回执 + Pump
// ============================================================================

namespace
{
constexpr Guid kDispatchPlayerId = 950300001;
constexpr SessionId kDispatchSession = 131201;

// 本宿主不加载配表:在测试创建点显式构造"加载已完成"的持久组件集合(与 cross_zone_test.cpp 的
// CreatePersistedPlayerComps 同一份清单;那边是匿名命名空间,不跨编译单元复用)。
void CreatePersistedPlayerComps(entt::entity player)
{
    auto& registry = tlsEcs.actorRegistry;
    registry.emplace<Transform>(player);
    registry.emplace<PlayerUint64Comp>(player);
    registry.emplace<PlayerSkillListComp>(player);
    registry.emplace<PlayerUint32Comp>(player);
    registry.emplace<BaseAttributesComp>(player);
    registry.emplace<LevelComp>(player);
    registry.emplace<PlayerAttributeComp>(player);
    registry.emplace<PlayerPetComp>(player);
    registry.emplace<PlayerProfileComp>(player);
    registry.emplace<PlayerBagsComp>(player);
    registry.emplace<MissionsContainerComp>(player);
    registry.emplace<PlayerAssetOpLedgerComp>(player);
    registry.emplace<BattleSettlementLedgerComp>(player);
    registry.emplace<PlayerInboxLedgerComp>(player);
    registry.emplace<CurrencyComp>(player);
    registry.emplace<PlayerCurrencyComp>(player);
}

// 在线(未退出)的玩家:绑定会话、写进 SessionMap。
entt::entity MakeOnlinePlayer()
{
    const auto player = tlsEcs.actorRegistry.create();
    CreatePersistedPlayerComps(player);
    tlsEcs.playerList.emplace(kDispatchPlayerId, player);
    tlsEcs.actorRegistry.emplace<PlayerGuidComp>(player, kDispatchPlayerId);
    auto& session = tlsEcs.actorRegistry.emplace<PlayerSessionSnapshotComp>(player);
    session.set_gate_session_id(kDispatchSession);
    session.set_player_id(kDispatchPlayerId);
    SessionMap().insert_or_assign(kDispatchSession, kDispatchPlayerId);
    return player;
}

// 退出存盘在途的玩家:UnregisterPlayer + 退出意图(退出会话就是当前会话)。logout_initiated_ms 留 0 = 跳过租约告警。
entt::entity MakeExitingPlayer()
{
    const auto player = MakeOnlinePlayer();
    tlsEcs.actorRegistry.emplace<UnregisterPlayer>(player);
    PlayerExitIntentComp intent;
    intent.sessionAtExit = kDispatchSession;
    intent.cause = ExitCause::kClientDisconnect;
    tlsEcs.actorRegistry.emplace<PlayerExitIntentComp>(player, intent);
    return player;
}

// 与 SavePlayerToRedis 写盘时同一个形状:marshal + 两张子表的 player_id。
bool MarshalAsSaved(entt::entity player, PlayerAllData& data)
{
    if (!PlayerAllDataMessageFieldsMarshal(player, data))
    {
        return false;
    }
    data.mutable_player_database_data()->set_player_id(kDispatchPlayerId);
    data.mutable_player_database_1_data()->set_player_id(kDispatchPlayerId);
    return true;
}

// 让快照等于当前内存(= Redis 里已是同一份):快路径不写盘,收敛核对判收敛。
bool MarkPersisted(entt::entity player)
{
    PlayerAllData saved;
    if (!MarshalAsSaved(player, saved))
    {
        return false;
    }
    tlsEcs.actorRegistry.emplace_or_replace<PlayerLastPersistedSnapshotComp>(player).Replace(saved);
    return true;
}

// 在全局账本上给玩家登记一次「两条 DBTask 都已入队、都在途」的存盘,返回第一张票据(第二张由 out 带回)。
db_task_dispatch::Ticket IssueQueuedSave(Guid playerId, db_task_dispatch::Ticket* second = nullptr,
                                         Clock::time_point issuedAt = Clock::now())
{
    auto& ledger = DbTaskDispatchSystem::LedgerForTest();
    const auto gen = ledger.BeginSave(playerId);
    const auto first = ledger.IssueTicket(playerId, gen, issuedAt);
    const auto other = ledger.IssueTicket(playerId, gen, issuedAt);
    ledger.FinishSave(playerId, gen, /*allQueued=*/true);
    if (second != nullptr)
    {
        *second = other;
    }
    return first;
}

std::uint64_t Delta(const db_task_dispatch_stats::Snapshot& before, Counter counter)
{
    return db_task_dispatch_stats::Read().Get(counter) - before.Get(counter);
}

class DbTaskDispatchFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tlsEcs.Clear();
        SessionMap().clear();
        DbTaskDispatchSystem::ResetForTest();
        DbTaskDispatchSystem::Initialize(&PlayerLifecycleSystem::OnDbTaskDispatchSettled);
    }

    void TearDown() override
    {
        // 清账本、注销监听器:cross_zone_test.cpp 的 ExitPersist* 用例依赖空账本(闸门恒判 kFinish)。
        DbTaskDispatchSystem::ResetForTest();
        DbTaskDispatchSystem::Shutdown();
        tlsEcs.Clear();
        SessionMap().clear();
    }
};

using DbTaskDispatchEcs = DbTaskDispatchFixture;
} // namespace

TEST_F(DbTaskDispatchEcs, LandedExitWaitsForInFlightAck)
{
    const auto player = MakeExitingPlayer();
    db_task_dispatch::Ticket second = db_task_dispatch::kNoTicket;
    const auto first = IssueQueuedSave(kDispatchPlayerId, &second);
    PlayerAllData landed;
    ASSERT_TRUE(MarshalAsSaved(player, landed));
    const auto before = db_task_dispatch_stats::Read();

    PlayerLifecycleSystem::HandlePlayerAsyncSaved(kDispatchPlayerId, landed);

    ASSERT_TRUE(tlsEcs.actorRegistry.valid(player)) << "Redis 收敛了,但 DBTask 还在途:不能销毁";
    const auto* intent = tlsEcs.actorRegistry.try_get<PlayerExitIntentComp>(player);
    ASSERT_NE(intent, nullptr);
    EXPECT_TRUE(intent->awaitingDbTaskAck);
    EXPECT_EQ(Delta(before, Counter::kExitAwaitAck), 1u);
    const auto* snapshot = tlsEcs.actorRegistry.try_get<PlayerLastPersistedSnapshotComp>(player);
    ASSERT_NE(snapshot, nullptr) << "这份字节确实已进 Redis,快照照常更新";
    ASSERT_TRUE(snapshot->HasSnapshot());
    EXPECT_EQ(snapshot->snapshot->SerializeAsString(), landed.SerializeAsString());

    DbTaskDispatchSystem::OnDeliveryReport(first, /*delivered=*/true);
    DbTaskDispatchSystem::Pump();
    ASSERT_TRUE(tlsEcs.actorRegistry.valid(player)) << "还有一张在途";

    DbTaskDispatchSystem::OnDeliveryReport(second, /*delivered=*/true);
    DbTaskDispatchSystem::Pump();
    EXPECT_FALSE(tlsEcs.actorRegistry.valid(player)) << "两条 DBTask 都 acks=all 了:收尾销毁";
    EXPECT_EQ(tlsEcs.playerList.count(kDispatchPlayerId), 0u);
    EXPECT_EQ(SessionMap().count(kDispatchSession), 0u);
    EXPECT_EQ(DbTaskDispatchSystem::LedgerForTest().TrackedPlayers(), 0u) << "DestroyPlayer 回收账本条目";
    EXPECT_EQ(Delta(before, Counter::kOrphanUnsynced), 0u);
}

TEST_F(DbTaskDispatchEcs, FastPathExitWaitsForInFlightAck)
{
    const auto player = MakeOnlinePlayer();
    ASSERT_TRUE(MarkPersisted(player));
    db_task_dispatch::Ticket second = db_task_dispatch::kNoTicket;
    const auto first = IssueQueuedSave(kDispatchPlayerId, &second);
    const auto before = db_task_dispatch_stats::Read();

    PlayerLifecycleSystem::HandleExitGameNode(player, ExitCause::kClientDisconnect);

    ASSERT_TRUE(tlsEcs.actorRegistry.valid(player)) << "快路径判 Redis 已是同一份,但上一次存盘的 DBTask 还在途";
    EXPECT_TRUE((tlsEcs.actorRegistry.all_of<UnregisterPlayer, PlayerExitIntentComp>(player)));
    EXPECT_TRUE(tlsEcs.actorRegistry.get<PlayerExitIntentComp>(player).awaitingDbTaskAck);
    EXPECT_TRUE(PlayerLifecycleSystem::IsSaveInFlight(kDispatchPlayerId)) << "停机 drain 据此继续等";
    EXPECT_EQ(Delta(before, Counter::kExitAwaitAck), 1u);

    // 两张票据都要落定。
    DbTaskDispatchSystem::OnDeliveryReport(first, /*delivered=*/true);
    DbTaskDispatchSystem::OnDeliveryReport(second, /*delivered=*/true);
    DbTaskDispatchSystem::Pump();

    EXPECT_FALSE(tlsEcs.actorRegistry.valid(player));
    EXPECT_EQ(tlsEcs.playerList.count(kDispatchPlayerId), 0u);
    EXPECT_EQ(SessionMap().count(kDispatchSession), 0u);
}

TEST_F(DbTaskDispatchEcs, FailedAckOnExitingPlayerRetainsWhenCapped)
{
    const auto player = MakeExitingPlayer();
    ASSERT_TRUE(MarkPersisted(player));
    {
        auto& intent = tlsEcs.actorRegistry.get<PlayerExitIntentComp>(player);
        intent.dbTaskResaveRounds = player_exit::kMaxExitDbTaskResaveRounds; // 前几轮重存都已失败
        intent.awaitingDbTaskAck = true;                                     // 已停在闸门上
    }
    db_task_dispatch::Ticket second = db_task_dispatch::kNoTicket;
    const auto first = IssueQueuedSave(kDispatchPlayerId, &second);
    const auto before = db_task_dispatch_stats::Read();

    DbTaskDispatchSystem::OnDeliveryReport(first, /*delivered=*/false);
    DbTaskDispatchSystem::Pump();

    ASSERT_TRUE(tlsEcs.actorRegistry.valid(player)) << "投递失败且重存已到上限:保留实体,不丢差额";
    EXPECT_TRUE((tlsEcs.actorRegistry.all_of<UnregisterPlayer, PlayerExitIntentComp>(player)));
    EXPECT_TRUE(tlsEcs.actorRegistry.get<PlayerExitIntentComp>(player).awaitingDbTaskAck);
    EXPECT_EQ(Delta(before, Counter::kDeliveryFailed), 1u);
    EXPECT_EQ(Delta(before, Counter::kExitResaveCapped), 1u);
    EXPECT_EQ(Delta(before, Counter::kExitResave), 0u) << "到上限不再重存(本宿主也不碰 Redis)";
    EXPECT_TRUE(DbTaskDispatchSystem::NeedsResave(kDispatchPlayerId));

    // 剩下那张成功了也补不回失败的那一份:仍然保留。
    DbTaskDispatchSystem::OnDeliveryReport(second, /*delivered=*/true);
    DbTaskDispatchSystem::Pump();
    EXPECT_TRUE(tlsEcs.actorRegistry.valid(player));
}

TEST_F(DbTaskDispatchEcs, ExhaustedRekickWithMissingComponentRetainsEntity)
{
    // 被闸门保留的实体再收到退出请求:轮次清零、重新开始重存;重存因持久组件缺失被拒 → fail-closed 保留,不收尾。
    const auto player = MakeExitingPlayer();
    ASSERT_TRUE(MarkPersisted(player));
    {
        auto& intent = tlsEcs.actorRegistry.get<PlayerExitIntentComp>(player);
        intent.dbTaskResaveRounds = player_exit::kMaxExitDbTaskResaveRounds;
        intent.awaitingDbTaskAck = true;
    }
    auto& ledger = DbTaskDispatchSystem::LedgerForTest();
    const auto gen = ledger.BeginSave(kDispatchPlayerId);
    ledger.MarkLocalFailure(kDispatchPlayerId, gen);
    ledger.FinishSave(kDispatchPlayerId, gen, /*allQueued=*/false);
    tlsEcs.actorRegistry.remove<PlayerCurrencyComp>(player); // 重存在 Redis 之前就被拒
    const auto before = db_task_dispatch_stats::Read();

    PlayerLifecycleSystem::HandleExitGameNode(player, ExitCause::kNodeShutdown);

    ASSERT_TRUE(tlsEcs.actorRegistry.valid(player));
    EXPECT_TRUE((tlsEcs.actorRegistry.all_of<UnregisterPlayer, PlayerExitIntentComp>(player)));
    const auto& intent = tlsEcs.actorRegistry.get<PlayerExitIntentComp>(player);
    EXPECT_EQ(intent.dbTaskResaveRounds, 1u) << "rekick 清零后闸门又重存了一轮";
    EXPECT_TRUE(intent.awaitingDbTaskAck);
    EXPECT_EQ(Delta(before, Counter::kExitRekick), 1u);
    EXPECT_EQ(Delta(before, Counter::kExitResave), 1u);
    EXPECT_EQ(Delta(before, Counter::kExitUnsynced), 0u) << "拒存不是「没法再补」:不带着落后收尾";
    EXPECT_EQ(SessionMap().count(kDispatchSession), 1u);
    EXPECT_TRUE(DbTaskDispatchSystem::NeedsResave(kDispatchPlayerId));
}

TEST_F(DbTaskDispatchEcs, OrphanFailureIsCountedAndForgotten)
{
    // 实体已经不在本节点(被废黜 / 交接放行之后),它最后一次存盘的 DBTask 才回执失败。
    db_task_dispatch::Ticket second = db_task_dispatch::kNoTicket;
    const auto first = IssueQueuedSave(kDispatchPlayerId, &second);
    DbTaskDispatchSystem::OnDeliveryReport(second, /*delivered=*/true);
    const auto before = db_task_dispatch_stats::Read();

    DbTaskDispatchSystem::OnDeliveryReport(first, /*delivered=*/false);
    DbTaskDispatchSystem::Pump();

    EXPECT_EQ(Delta(before, Counter::kOrphanUnsynced), 1u);
    EXPECT_EQ(DbTaskDispatchSystem::LedgerForTest().TrackedPlayers(), 0u);
}

TEST_F(DbTaskDispatchEcs, DeliveredOrphanIsForgottenSilently)
{
    db_task_dispatch::Ticket second = db_task_dispatch::kNoTicket;
    const auto first = IssueQueuedSave(kDispatchPlayerId, &second);
    const auto before = db_task_dispatch_stats::Read();

    DbTaskDispatchSystem::OnDeliveryReport(first, /*delivered=*/true);
    DbTaskDispatchSystem::OnDeliveryReport(second, /*delivered=*/true);
    DbTaskDispatchSystem::Pump();

    EXPECT_EQ(Delta(before, Counter::kOrphanUnsynced), 0u);
    EXPECT_EQ(DbTaskDispatchSystem::LedgerForTest().TrackedPlayers(), 0u);
}

TEST_F(DbTaskDispatchEcs, EmptyLedgerKeepsExistingFinishBehavior)
{
    const auto player = MakeExitingPlayer();
    PlayerAllData landed;
    ASSERT_TRUE(MarshalAsSaved(player, landed));
    const auto before = db_task_dispatch_stats::Read();

    PlayerLifecycleSystem::HandlePlayerAsyncSaved(kDispatchPlayerId, landed);

    EXPECT_FALSE(tlsEcs.actorRegistry.valid(player)) << "没有在途 DBTask、也不欠 MySQL:与改动前一样落地即收尾";
    EXPECT_EQ(SessionMap().count(kDispatchSession), 0u);
    EXPECT_EQ(Delta(before, Counter::kExitAwaitAck), 0u);
}

TEST_F(DbTaskDispatchEcs, SendDbTaskWithoutBrokersMarksResave)
{
    ASSERT_FALSE(KafkaProducer::Instance().brokersConfigured())
        << "前置条件:本测试线程的生产者没登记 broker,发送必然被同步拒绝";
    const auto before = db_task_dispatch_stats::Read();

    const auto gen = DbTaskDispatchSystem::BeginSave(kDispatchPlayerId);
    const auto sent = DbTaskDispatchSystem::SendDbTask(kDispatchPlayerId, gen, "db_task_zone_1", "payload",
                                                       std::to_string(kDispatchPlayerId));
    DbTaskDispatchSystem::FinishSave(kDispatchPlayerId, gen, /*allQueued=*/sent.queued);

    EXPECT_FALSE(sent.queued);
    EXPECT_FALSE(sent.error.empty());
    EXPECT_TRUE(DbTaskDispatchSystem::NeedsResave(kDispatchPlayerId)) << "没入队 = 欠 MySQL 一次整份写";
    EXPECT_EQ(DbTaskDispatchSystem::InFlight(kDispatchPlayerId), 0u) << "没入队的不算在途,永远不会有回执";
    EXPECT_EQ(DbTaskDispatchSystem::InFlightTickets(), 0u);
    EXPECT_EQ(KafkaProducer::Instance().trackedInFlight(), 0u) << "没入队就不登记关联号";
    EXPECT_EQ(Delta(before, Counter::kProduceRejected), 1u);
}

TEST_F(DbTaskDispatchEcs, ExpiredTicketDuringPumpCountsAndEnqueues)
{
    // 回执始终不来:超过 kTicketTimeout 按投递失败处理,玩家进待办(实体不在 → 回收并计孤儿)。
    const auto longAgo = Clock::now() - DbTaskDispatchSystem::kTicketTimeout - std::chrono::seconds(1);
    db_task_dispatch::Ticket second = db_task_dispatch::kNoTicket;
    IssueQueuedSave(kDispatchPlayerId, &second, longAgo);
    const auto before = db_task_dispatch_stats::Read();

    DbTaskDispatchSystem::Pump();

    EXPECT_EQ(Delta(before, Counter::kTicketExpired), 2u);
    EXPECT_EQ(DbTaskDispatchSystem::InFlightTickets(), 0u);
    EXPECT_EQ(Delta(before, Counter::kOrphanUnsynced), 1u);
    EXPECT_EQ(DbTaskDispatchSystem::LedgerForTest().TrackedPlayers(), 0u);

    // 超时之后迟到的真回执:票据已销,什么都不改。
    DbTaskDispatchSystem::OnDeliveryReport(second, /*delivered=*/true);
    DbTaskDispatchSystem::Pump();
    EXPECT_EQ(DbTaskDispatchSystem::LedgerForTest().TrackedPlayers(), 0u);
}
