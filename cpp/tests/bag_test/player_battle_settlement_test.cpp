#include <gtest/gtest.h>
#include "engine/core/type_define/player_guid_comp.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "modules/condition/condition_type.h"
#include "modules/mission/comp/mission_comp.h"
#include "modules/bag/bag_system.h"
#include "modules/bag/comp/player_bags_comp.h"
#include "modules/bag/item_system.h"
#include "modules/currency/comp/player_currency_comp.h"
#include "modules/currency/constants/currency.h"
#include "modules/currency/system/currency_system.h"
#include "proto/battle/battle_data.pb.h"
#include "table/code/item_table.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/actor_attribute_state_comp.pb.h"
#include "proto/common/component/player_comp.pb.h"
#include "proto/common/component/player_login_comp.pb.h" // LOGIN_FIRST
#include "proto/common/component/player_pet_comp.pb.h"
#include "proto/common/component/player_skill_comp.pb.h"
#include "proto/common/component/battle_comp.pb.h"
#include "proto/common/component/battle_settlement_ledger_comp.pb.h"
#include "services/scene/battle/system/battle_settlement_ledger.h"
#include "services/scene/player/comp/player_frozen_comp.h"
#include "services/scene/player/comp/player_ownership_comp.h"
#include "services/scene/player/comp/last_persisted_snapshot_comp.h"
#include "proto/common/component/currency_comp.pb.h"
#include "proto/common/event/battle_event.pb.h"
#include "modules/mission/mission_events.h"
#include "services/battle/settlement/settlement_outbox.h"
#include "services/scene/battle/system/player_battle.h"
#include "thread_context/ecs_context.h"

// 访问生产共用入口；不复制金币和任务副作用实现，不连接 Redis 或登录账号。
class PlayerBattleSettlementTestAccess {
public:
    static bool Snapshot(entt::entity player, BattlePlayerSnapshot& snapshot, uint32_t& errorTipId) {
        return PlayerBattleSystem::BuildBattleSnapshot(player, snapshot, errorTipId);
    }
    static bool Apply(entt::entity player, const BattleSettlementData& settlement) {
        return PlayerBattleSystem::ApplySettlementToEntity(player, settlement);
    }
    static bool Apply(entt::entity player, const BattleSettlementData& settlement,
                      bool& alreadyAppliedOut) {
        return PlayerBattleSystem::ApplySettlementToEntity(player, settlement, &alreadyAppliedOut);
    }
    static void ApplyPending(entt::entity player, const BattleSettlementData& settlement) {
        BattleSettlementEvent event;
        *event.mutable_settlement() = settlement;
        PlayerBattleSystem::ApplyPendingSettlement(player, event);
    }
    static void RecoverAfterFreezeExpiry(entt::entity player, uint64_t playerId) {
        PlayerBattleSystem::RecoverPendingSettlementAfterFreezeExpiry(player, playerId);
    }
    static bool LoginRecheckPending(entt::entity player) {
        return PlayerBattleSystem::IsLoginRecheckPending(player);
    }
    static void RunLoginRechecks() {
        PlayerBattleSystem::RunLoginRechecks();
    }
};

namespace {

// 结算应用成功后会立刻压一次存盘(真实现是 PlayerLifecycleSystem::SavePlayerToRedis)。
// 单测不连 Redis,注入一个只计数的假实现;返回 false = 本次没有产生写入,
// 于是 durable 判定只能落在 PlayerLastPersistedSnapshotComp 上 —— 单测里它从不存在,
// 正是「已应用但尚未落盘」这个最关键的中间态。
int gPersistCalls = 0;
bool CountingPersist(entt::entity) {
    ++gPersistCalls;
    return false;
}

class PlayerBattleSettlementTest : public testing::Test {
protected:
    void SetUp() override {
        // 缓存跨实体生命周期，所以每个测试/重复运行分配不同真实身份。
        static uint64_t nextPlayer = 9876500;
        playerId = ++nextPlayer;
        CreatePlayerEntity();
        RegisterPlayer();
        settlement.set_player_id(playerId);
        settlement.set_battle_id(701);
        settlement.set_outcome(BATTLE_OUTCOME_SIDE_A_WIN);
        settlement.set_health(80);
        settlement.set_mana(20);
        settlement.set_gold_gain(12);
        for (const auto configId : {uint32_t{7002}, uint32_t{7001}}) {
            auto* defeat = settlement.add_defeated_monsters();
            defeat->set_monster_config_id(configId);
            defeat->set_count(1);
        }
        tlsEcs.dispatcher.sink<ConditionEvent>().connect<&PlayerBattleSettlementTest::OnCondition>(*this);
        gPersistCalls = 0;
        PlayerBattleSystem::SetPersistFnForTest(&CountingPersist);
    }
    // 销账入口 AckSettlementPending 按 player_id 现取实体(tlsEcs.GetPlayer),不登记就走不到
    // 「活实体 + 账本」那几条规则。重建实体后必须重新登记。
    void RegisterPlayer() {
        tlsEcs.playerList.erase(playerId);
        tlsEcs.playerList.emplace(playerId, player);
    }
    void TearDown() override {
        tlsEcs.dispatcher.sink<ConditionEvent>().disconnect<&PlayerBattleSettlementTest::OnCondition>(*this);
        PlayerBattleSystem::SetPersistFnForTest(nullptr);
        tlsEcs.playerList.erase(playerId);
        if (tlsEcs.actorRegistry.valid(player)) tlsEcs.actorRegistry.destroy(player);
    }
    void CreatePlayerEntity() {
        player = tlsEcs.actorRegistry.create();
        tlsEcs.actorRegistry.emplace<PlayerGuidComp>(player, Guid{playerId});
        auto& attributes = tlsEcs.actorRegistry.emplace<BaseAttributesComp>(player);
        attributes.set_health(100);
        attributes.set_mana(50);
        auto& derived = tlsEcs.actorRegistry.emplace<DerivedAttributesComp>(player);
        derived.set_max_health(100);
        derived.set_max_mana(50);
        tlsEcs.actorRegistry.emplace<CurrencyComp>(player);
        tlsEcs.actorRegistry.emplace<PlayerCurrencyComp>(player);
        tlsEcs.actorRegistry.emplace<MissionsContainerComp>(player);
        // 结算幂等账本是加载时(PlayerDatabaseMessageFieldsUnmarshal)无条件挂上的必需组件,
        // 结算路径只取不建。夹具照加载路径挂一本空账本 = 「盘上没有任何已应用条目」。
        tlsEcs.actorRegistry.emplace<BattleSettlementLedgerComp>(player);
    }
    void OnCondition(const ConditionEvent& event) {
        if (event.player != player) return;
        EXPECT_EQ(event.conditionType, static_cast<uint32_t>(eConditionType::kConditionKillMonster));
        EXPECT_EQ(event.amount, 1);
        if (event.conditionIds.size() == 1) kills.push_back(event.conditionIds.at(0));
        if (tryReentry) {
            tryReentry = false;
            didReenter = true;
            reentryApplied = PlayerBattleSettlementTestAccess::Apply(player, settlement);
        }
    }
    entt::entity player{entt::null};
    uint64_t playerId{};
    BattleSettlementData settlement;
    std::vector<uint32_t> kills;
    bool tryReentry{}, didReenter{}, reentryApplied{};
};

TEST_F(PlayerBattleSettlementTest, DuplicateSuccessfulCallbacksAndReentryPayAndProgressOnce) {
    tryReentry = true;
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_TRUE(didReenter);
    EXPECT_FALSE(reentryApplied);
    // 模拟两个 GET 均返回同一 battle_id，DEL 尚未执行时第二个回调抵达共用入口。
    // 改造后返回 true(= 这一局已应用,调用方可以解冻并尝试销账),但出参会告诉调用方
    // 「奖是上一次发的」,据此不再推一次结算面板;钱和进度当然不能再动。
    bool alreadyApplied = false;
    EXPECT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement, alreadyApplied));
    EXPECT_TRUE(alreadyApplied);
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
    EXPECT_EQ(kills, (std::vector<uint32_t>{7002, 7001}));
    EXPECT_EQ(tlsEcs.actorRegistry.get<BaseAttributesComp>(player).health(), 80u);
    // 下一局不依赖 id 单调递增，正常收益仍然可应用。
    settlement.set_battle_id(700);
    EXPECT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 24u);
    EXPECT_EQ(kills.size(), 4u);
}

TEST_F(PlayerBattleSettlementTest, CurrencyFailureLeavesPendingSideEffectsUntouchedAndCanRetry) {
    tlsEcs.actorRegistry.remove<CurrencyComp>(player);
    tlsEcs.actorRegistry.emplace<InBattleComp>(player).set_battle_id(settlement.battle_id());
    PlayerBattleSettlementTestAccess::ApplyPending(player, settlement);
    EXPECT_EQ(tlsEcs.actorRegistry.get<BaseAttributesComp>(player).health(), 100u);
    EXPECT_EQ(tlsEcs.actorRegistry.get<BaseAttributesComp>(player).mana(), 50u);
    EXPECT_TRUE(kills.empty());
    EXPECT_TRUE(tlsEcs.actorRegistry.all_of<InBattleComp>(player));
    tlsEcs.actorRegistry.emplace<CurrencyComp>(player);
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
    EXPECT_EQ(kills.size(), 2u);
}

// 空宠物列表是合法持久状态；整个组件缺失则是加载故障，不能跳过宝宝后仍发金币、记整笔账本。
TEST_F(PlayerBattleSettlementTest, MissingPetComponentRejectsBeforeCurrencyHealthAndLedgerChanges) {
    settlement.add_pets()->set_pet_id(901);
    ASSERT_FALSE(tlsEcs.actorRegistry.all_of<PlayerPetComp>(player));

    EXPECT_FALSE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));
    EXPECT_EQ(100u, tlsEcs.actorRegistry.get<BaseAttributesComp>(player).health());
    EXPECT_EQ(50u, tlsEcs.actorRegistry.get<BaseAttributesComp>(player).mana());
    EXPECT_TRUE(kills.empty());
    EXPECT_FALSE(battle_settlement::HasApplied(
        tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player), settlement.battle_id()));
    EXPECT_FALSE(tlsEcs.actorRegistry.all_of<PlayerPetComp>(player));
}

TEST_F(PlayerBattleSettlementTest, MissingBagComponentRejectsBeforeCurrencyHealthAndLedgerChanges) {
    auto* gained = settlement.add_items_gained();
    gained->set_item_table_id(7001);
    gained->set_count(1);
    ASSERT_FALSE(tlsEcs.actorRegistry.all_of<PlayerBagsComp>(player));

    EXPECT_FALSE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));
    EXPECT_EQ(100u, tlsEcs.actorRegistry.get<BaseAttributesComp>(player).health());
    EXPECT_TRUE(kills.empty());
    EXPECT_FALSE(battle_settlement::HasApplied(
        tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player), settlement.battle_id()));
    EXPECT_FALSE(tlsEcs.actorRegistry.all_of<PlayerBagsComp>(player));
}

TEST_F(PlayerBattleSettlementTest, MissingDerivedAttributesRejectsBeforeCurrencyAndCanRetryAfterRecovery) {
    tlsEcs.actorRegistry.remove<DerivedAttributesComp>(player);

    EXPECT_FALSE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));
    EXPECT_EQ(100u, tlsEcs.actorRegistry.get<BaseAttributesComp>(player).health());
    EXPECT_TRUE(kills.empty());
    EXPECT_FALSE(battle_settlement::HasApplied(
        tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player), settlement.battle_id()));
    EXPECT_FALSE(tlsEcs.actorRegistry.all_of<DerivedAttributesComp>(player));

    auto& derived = tlsEcs.actorRegistry.emplace<DerivedAttributesComp>(player);
    derived.set_max_health(100);
    derived.set_max_mana(50);
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(12u, CurrencySystem::GetBalance(player, kCurrencyGold));
    EXPECT_EQ(80u, tlsEcs.actorRegistry.get<BaseAttributesComp>(player).health());
}

TEST_F(PlayerBattleSettlementTest, MissingMissionComponentRejectsBeforeCurrencyProgressAndLedgerChanges) {
    tlsEcs.actorRegistry.remove<MissionsContainerComp>(player);

    EXPECT_FALSE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(0u, CurrencySystem::GetBalance(player, kCurrencyGold));
    EXPECT_EQ(100u, tlsEcs.actorRegistry.get<BaseAttributesComp>(player).health());
    EXPECT_TRUE(kills.empty());
    EXPECT_FALSE(battle_settlement::HasApplied(
        tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player), settlement.battle_id()));
    EXPECT_FALSE(tlsEcs.actorRegistry.all_of<MissionsContainerComp>(player));
}

TEST_F(PlayerBattleSettlementTest, SnapshotRejectsMissingPersistentOrDerivedInputsBeforeChangingOutput) {
    auto& registry = tlsEcs.actorRegistry;
    registry.emplace<LevelComp>(player);
    registry.emplace<PlayerUint32Comp>(player);
    registry.emplace<PlayerProfileComp>(player);
    registry.emplace<PlayerSkillListComp>(player);
    registry.emplace<PlayerPetComp>(player);
    registry.emplace<PlayerBagsComp>(player);

    const auto expectRefusedWithout = [&]<typename Comp>() {
        registry.remove<Comp>(player);
        BattlePlayerSnapshot snapshot;
        snapshot.set_player_id(901);
        uint32_t errorTipId = kSuccess;
        EXPECT_FALSE(PlayerBattleSettlementTestAccess::Snapshot(player, snapshot, errorTipId));
        EXPECT_EQ(kServiceUnavailable, errorTipId);
        EXPECT_EQ(901u, snapshot.player_id());
        EXPECT_FALSE(registry.all_of<Comp>(player));
        registry.emplace<Comp>(player);
    };
    expectRefusedWithout.template operator()<LevelComp>();
    expectRefusedWithout.template operator()<PlayerUint32Comp>();
    expectRefusedWithout.template operator()<PlayerProfileComp>();
    expectRefusedWithout.template operator()<PlayerSkillListComp>();
    expectRefusedWithout.template operator()<PlayerPetComp>();
    expectRefusedWithout.template operator()<PlayerBagsComp>();
    expectRefusedWithout.template operator()<DerivedAttributesComp>();
}

// 真实重登:资产与账本来自同一份落盘 blob(不变量 I3),所以账本要和金币一起带过去。
// 登录补应用与 10s 后发件箱的再一次投递都必须被账本挡住,且不能摘掉玩家新一局的冻结。
TEST_F(PlayerBattleSettlementTest, ReloginOldPendingDoesNotPayAgainOrRemoveNewBattle) {
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    const auto currency = tlsEcs.actorRegistry.get<CurrencyComp>(player);
    const auto attributes = tlsEcs.actorRegistry.get<BaseAttributesComp>(player);
    const auto ledger = tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player);
    tlsEcs.actorRegistry.destroy(player);
    CreatePlayerEntity();
    RegisterPlayer();
    tlsEcs.actorRegistry.get<CurrencyComp>(player) = currency;
    tlsEcs.actorRegistry.get<BaseAttributesComp>(player) = attributes;
    tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player) = ledger;
    tlsEcs.actorRegistry.emplace<InBattleComp>(player).set_battle_id(702);
    gPersistCalls = 0;
    for (int delivery = 0; delivery < 2; ++delivery) {
        PlayerBattleSettlementTestAccess::ApplyPending(player, settlement);
        EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u) << "delivery " << delivery;
        EXPECT_EQ(kills.size(), 2u) << "delivery " << delivery;
        const auto* current = tlsEcs.actorRegistry.try_get<InBattleComp>(player);
        ASSERT_NE(current, nullptr);
        EXPECT_EQ(current->battle_id(), 702u);
    }
    // 两次都是「已应用、未落盘」:不销账,各压一次存盘把账本推上盘。
    EXPECT_EQ(gPersistCalls, 2);
}

// 进程内缓存说应用过、实体账本里却没有:只可能是那次应用没落盘、实体被重建过 ——
// 资产也没落盘,这一局必须在**这一次**投递就重新应用(登录钩子只投这一次,不能指望下一次),
// 而且只能一次。
TEST_F(PlayerBattleSettlementTest, CacheHitButLedgerMissReappliesExactlyOnce) {
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    ASSERT_EQ(kills.size(), 2u);
    tlsEcs.actorRegistry.destroy(player);
    CreatePlayerEntity();  // 从「没落盘」的旧 blob 重建:没有这一局的金币,也没有账本条目
    RegisterPlayer();

    PlayerBattleSettlementTestAccess::ApplyPending(player, settlement);  // 按账本清掉陈旧缓存,当场应用
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
    EXPECT_EQ(kills.size(), 4u);
    EXPECT_TRUE(battle_settlement::HasApplied(
        tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player), settlement.battle_id()));

    PlayerBattleSettlementTestAccess::ApplyPending(player, settlement);  // 再投一次:账本挡住
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
    EXPECT_EQ(kills.size(), 4u);
}

TEST_F(PlayerBattleSettlementTest, MissingAttributesCanRetryWithoutPrematureGoldCredit) {
    tlsEcs.actorRegistry.remove<BaseAttributesComp>(player);
    EXPECT_FALSE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 0u);
    EXPECT_TRUE(kills.empty());
    tlsEcs.actorRegistry.emplace<BaseAttributesComp>(player).set_health(100);
    EXPECT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
}

TEST_F(PlayerBattleSettlementTest, WrongPlayerCannotApplyOrPoisonValidRetry) {
    settlement.set_player_id(playerId + 1);
    EXPECT_FALSE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 0u);
    EXPECT_TRUE(kills.empty());
    settlement.set_player_id(playerId);
    EXPECT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
}
} // namespace

// ---------------------------------------------------------------------------
// 2026-09-25 §9.3:结算幂等的持久基底。
// 改造前幂等只有 thread_local 应用缓存,进程一重启就空,于是「应用 -> 当场销账 ->
// 存盘前崩溃」会把奖励永久吞掉。现在「这一局已应用」与资产写在同一份 player_database
// blob 里,同生共死。这一组用例守的就是那条不变量。
// ---------------------------------------------------------------------------

namespace {

TEST_F(PlayerBattleSettlementTest, AppliedSettlementIsRecordedInPersistedLedger) {
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    const auto* ledger = tlsEcs.actorRegistry.try_get<BattleSettlementLedgerComp>(player);
    ASSERT_NE(ledger, nullptr);
    EXPECT_TRUE(battle_settlement::HasApplied(*ledger, settlement.battle_id()));
    // 账本条目必须与资产在同一次存盘里落地,所以应用成功后必须立刻压一次存盘。
    EXPECT_EQ(gPersistCalls, 1);
}

// 进程重启 / 实体重建之后,「已应用」只能靠从盘上回来的账本知道。
// 这里直接预置账本(= 盘上那份),应用缓存对这个 battle_id 一无所知,
// 于是唯一能拦住重复发奖的就是持久账本本身。
TEST_F(PlayerBattleSettlementTest, LedgerFromDiskBlocksReapplyAfterRestart) {
    settlement.set_battle_id(7031);
    auto& ledger = tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player);
    battle_settlement::RecordApplied(ledger, settlement.battle_id(), 1234);

    bool alreadyApplied = false;
    EXPECT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement, alreadyApplied));
    EXPECT_TRUE(alreadyApplied);
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 0u);
    EXPECT_TRUE(kills.empty());
    EXPECT_EQ(tlsEcs.actorRegistry.get<BaseAttributesComp>(player).health(), 100u);
}

// 冻结期必须整笔延后,而不是只挡金币和道具。
// 一笔「只有 HP + 击杀事实、没有金币也没有掉落」的 PVE 结算(打空怪,完全正常)
// 改造前会穿过那道只看 gold/items 的窄闸:任务进度被推、血量被改,而此刻存盘被硬性
// 跳过,改动随实体销毁一起没,账本却记成已应用 —— 等于把进度白送出去又丢掉。
TEST_F(PlayerBattleSettlementTest, FrozenPlayerDefersSettlementWithNoGoldOrItems) {
    settlement.set_gold_gain(0);
    tlsEcs.actorRegistry.emplace<PlayerFrozenComp>(player);

    EXPECT_FALSE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_TRUE(kills.empty());
    EXPECT_EQ(tlsEcs.actorRegistry.get<BaseAttributesComp>(player).health(), 100u);
    // 账本是加载时挂上的(夹具同此),这一局绝不能被登记进去。
    EXPECT_FALSE(battle_settlement::HasApplied(
        tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player), settlement.battle_id()));
    EXPECT_EQ(gPersistCalls, 0);

    // 解冻后照常应用,奖励一次不差 —— 延后不是丢弃。
    tlsEcs.actorRegistry.remove<PlayerFrozenComp>(player);
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(kills.size(), 2u);
}

// 归属交接一挂上,SavePlayerToRedis 开头就直接跳过写盘。此时应用等于改了一份永远落不了盘
// 的内存态,必须整笔延后,交给发件箱重投或目的地节点补应用。
TEST_F(PlayerBattleSettlementTest, TravelHandoffDefersWholeSettlement) {
    auto& handoff = tlsEcs.actorRegistry.emplace<PlayerTravelHandoffComp>(player);
    handoff.requestedAtMs = 42;

    EXPECT_FALSE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 0u);
    EXPECT_TRUE(kills.empty());
    EXPECT_FALSE(battle_settlement::HasApplied(
        tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player), settlement.battle_id()));
}

// 「已应用但未落盘」时销账必须延后:这一刻销账,崩溃即永久丢奖励(§9.3)。
// 可观察的证据是销账入口又压了一次存盘(应用时一次 + 销账延后一次),账本条目也不能被摘。
TEST_F(PlayerBattleSettlementTest, AckDefersClearWhileAppliedButNotDurable) {
    tlsEcs.actorRegistry.emplace<InBattleComp>(player).set_battle_id(settlement.battle_id());
    PlayerBattleSettlementTestAccess::ApplyPending(player, settlement);

    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
    EXPECT_EQ(gPersistCalls, 2);
    EXPECT_TRUE(battle_settlement::HasApplied(
        tlsEcs.actorRegistry.get<BattleSettlementLedgerComp>(player), settlement.battle_id()));
    // 冻结照常解除(锁留到落盘,但组件当场摘):玩家不会被卡在已结束的战斗里
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<InBattleComp>(player));
}

// 落盘快照里已有这一局 = 字节确实在盘上:走销账分支,不再额外压存盘。
TEST_F(PlayerBattleSettlementTest, AckClearsOnceLedgerEntryIsDurable) {
    PlayerAllData persisted;
    persisted.mutable_player_database_data()->mutable_settlement_ledger()->add_applied()->set_battle_id(
        settlement.battle_id());
    tlsEcs.actorRegistry.emplace<PlayerLastPersistedSnapshotComp>(player).Replace(persisted);

    PlayerBattleSettlementTestAccess::ApplyPending(player, settlement);

    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
    EXPECT_EQ(gPersistCalls, 1);  // 只有应用时那一次
}

// 「已有未落地存盘就不再压」依赖真实 Redis 客户端的队列状态,单测里客户端为空,
// 留给 battle-smoke 按日志核对(每场结算只应出现一次结算存盘)。

// 账本缺失 = 加载不变量被破坏(docs/design/ecs-entt-misuse-audit-20261015.md F036)。
// 修复前应用入口 get_or_emplace 一本空账本:把「缺账本」当成「没应用过」照发奖励,还把空账本
// 挂回实体(下一次存盘会拿它覆盖盘上的条目)。修复后整笔不应用、零副作用、不补建;
// 账本按加载路径回来之后照常应用一次 —— 延后不是丢弃。
TEST_F(PlayerBattleSettlementTest, MissingLedgerRejectsSettlementWithoutSideEffects) {
    tlsEcs.actorRegistry.remove<BattleSettlementLedgerComp>(player);

    // 修复前这几条都红:返回 true、金币 12、击杀 2 条、血量 80、账本被补建、压了一次存盘。
    EXPECT_FALSE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 0u);
    EXPECT_TRUE(kills.empty());
    EXPECT_EQ(tlsEcs.actorRegistry.get<BaseAttributesComp>(player).health(), 100u);
    EXPECT_EQ(gPersistCalls, 0);
    // ASSERT 而非 EXPECT:修复前账本已被补建,下面的 emplace 会触发 entt 断言把整个测试进程带崩。
    ASSERT_FALSE(tlsEcs.actorRegistry.any_of<BattleSettlementLedgerComp>(player));

    tlsEcs.actorRegistry.emplace<BattleSettlementLedgerComp>(player);
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
    EXPECT_EQ(kills.size(), 2u);
}

// 销账入口同理:活实体缺账本时「已应用 / 没应用」都判不了。修复前补建一本空账本并按「没应用」
// 当场销账。单测没有 Redis,销账命令本身发不出去,可观察的差别是账本有没有被补建。
TEST_F(PlayerBattleSettlementTest, MissingLedgerAckDoesNotFabricateLedger) {
    tlsEcs.actorRegistry.remove<BattleSettlementLedgerComp>(player);
    // 玩家已在下一局:这条迟到结算按 battle_id 不匹配走销账入口(ApplySettlement 的在线分支)。
    const uint64_t nextBattleId = settlement.battle_id() + 1;
    tlsEcs.actorRegistry.emplace<InBattleComp>(player).set_battle_id(nextBattleId);
    BattleSettlementEvent event;
    *event.mutable_settlement() = settlement;

    PlayerBattleSystem::ApplySettlement(event);

    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<BattleSettlementLedgerComp>(player));  // 修复前红
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 0u);
    const auto* current = tlsEcs.actorRegistry.try_get<InBattleComp>(player);
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->battle_id(), nextBattleId);
}

// ── 账本纯规则(无 ECS、无 Redis)────────────────────────────────────────────

TEST(BattleSettlementLedgerRule, RecordIsIdempotentAndForgetRemoves) {
    BattleSettlementLedgerComp ledger;
    EXPECT_FALSE(battle_settlement::HasApplied(ledger, 9u));
    EXPECT_FALSE(battle_settlement::RecordApplied(ledger, 9u, 100));
    EXPECT_FALSE(battle_settlement::RecordApplied(ledger, 9u, 200));
    EXPECT_EQ(ledger.applied_size(), 1);          // 重投重复登记不产生第二条
    EXPECT_EQ(ledger.applied(0).applied_at_ms(), 200u);
    EXPECT_TRUE(battle_settlement::HasApplied(ledger, 9u));
    EXPECT_TRUE(battle_settlement::ForgetApplied(ledger, 9u));
    EXPECT_FALSE(battle_settlement::ForgetApplied(ledger, 9u));
    EXPECT_FALSE(battle_settlement::HasApplied(ledger, 9u));
}

TEST(BattleSettlementLedgerRule, ZeroBattleIdIsNeverRecordedOrMatched) {
    BattleSettlementLedgerComp ledger;
    EXPECT_FALSE(battle_settlement::RecordApplied(ledger, 0u, 1));
    EXPECT_EQ(ledger.applied_size(), 0);
    EXPECT_FALSE(battle_settlement::HasApplied(ledger, 0u));
}

// 满员时淘汰**最旧**的一条,顺序由 applied_at_ms 决定而不是下标(AGENTS.md §11.6)。
// 刻意把最旧那条放在中间,下标顺序与时间顺序不一致时才能测出来。
TEST(BattleSettlementLedgerRule, EvictsOldestByTimestampNotByIndex) {
    BattleSettlementLedgerComp ledger;
    for (int i = 0; i < battle_settlement::kMaxAppliedRecords; ++i) {
        const uint64_t at = (i == 3) ? 1u : static_cast<uint64_t>(1000 + i);
        EXPECT_FALSE(battle_settlement::RecordApplied(ledger, static_cast<uint64_t>(i + 1), at));
    }
    ASSERT_EQ(ledger.applied_size(), battle_settlement::kMaxAppliedRecords);

    EXPECT_TRUE(battle_settlement::RecordApplied(ledger, 999u, 5000));  // 溢出 -> 淘汰
    EXPECT_EQ(ledger.applied_size(), battle_settlement::kMaxAppliedRecords);
    EXPECT_FALSE(battle_settlement::HasApplied(ledger, 4u));  // 下标 3、时间戳最小的那条
    EXPECT_TRUE(battle_settlement::HasApplied(ledger, 1u));
    EXPECT_TRUE(battle_settlement::HasApplied(ledger, 999u));
}

} // namespace

// ---------------------------------------------------------------------------
// 2026-09-17 G1/G3:结算道具落地(docs/design/turn-battle-gap-closure.md)
// 覆盖:消耗按实际持有夹紧、掉落真入包、道具失败不得连累金币入账。
// 物品 id 用真实 Item 表里的可叠加物(10 / 11);bag_test 的 main 已 Load 过 ItemTable
// 并装好 item 号段基线。
// ---------------------------------------------------------------------------

namespace {

constexpr uint32_t kSettlementStackItem = 10;   // 可叠加
constexpr uint32_t kSettlementDropItem = 11;    // 可叠加

class PlayerBattleSettlementItemTest : public PlayerBattleSettlementTest {
protected:
    void SetUp() override {
        PlayerBattleSettlementTest::SetUp();
        ItemTableManager::Instance().Load();
        auto& bags = tlsEcs.actorRegistry.emplace<PlayerBagsComp>(player);
        for (auto& bag : bags.bags) bag.SetPlayerGuid(Guid{playerId});
        bags.bags[kInventory].ExpandCapacity(20);
    }
    Bag& Inventory() {
        return tlsEcs.actorRegistry.get<PlayerBagsComp>(player).bags[kInventory];
    }
    void GiveItem(uint32_t configId, uint32_t count) {
        InitItemParam param;
        param.itemPBComp.set_config_id(configId);
        param.itemPBComp.set_size(count);
        ASSERT_EQ(kSuccess, Inventory().AddItem(param));
    }
    void SetConsumed(uint32_t configId, uint64_t count) {
        auto* entry = settlement.add_items_consumed();
        entry->set_item_table_id(configId);
        entry->set_count(count);
    }
    void SetGained(uint32_t configId, uint64_t count) {
        auto* entry = settlement.add_items_gained();
        entry->set_item_table_id(configId);
        entry->set_count(count);
    }
};

TEST_F(PlayerBattleSettlementItemTest, ConsumedIsDeductedAndGainedIsAddedToInventory) {
    GiveItem(kSettlementStackItem, 5);
    SetConsumed(kSettlementStackItem, 2);
    SetGained(kSettlementDropItem, 3);

    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));

    EXPECT_EQ(Inventory().GetTotalItemCount(kSettlementStackItem), 3u);
    EXPECT_EQ(Inventory().GetTotalItemCount(kSettlementDropItem), 3u);
    // 金币照常入账:道具与金币在同一次应用里
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
}

TEST_F(PlayerBattleSettlementItemTest, ConsumedIsClampedWhenPlayerNoLongerHoldsEnough) {
    // 战斗中途玩家的药被别处扣走了(今天已被局中闸挡住,这里验的是防守姿态):
    // 契约是"按实际持有校验扣除,不足按 0",绝不能整笔结算失败。
    GiveItem(kSettlementStackItem, 1);
    SetConsumed(kSettlementStackItem, 4);

    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));

    EXPECT_EQ(Inventory().GetTotalItemCount(kSettlementStackItem), 0u);
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
}

TEST_F(PlayerBattleSettlementItemTest, ConsumedForItemPlayerDoesNotHaveDoesNotFailSettlement) {
    SetConsumed(kSettlementStackItem, 2);
    SetGained(kSettlementDropItem, 1);

    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));

    EXPECT_EQ(Inventory().GetTotalItemCount(kSettlementStackItem), 0u);
    EXPECT_EQ(Inventory().GetTotalItemCount(kSettlementDropItem), 1u);
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
}

TEST_F(PlayerBattleSettlementItemTest, ConsumedNonBattleItemIsRejectedNotDestroyed) {
    // 反向校验:快照只会把 battle_usable 的物品放进副本,账本里出现别的 id 只可能是
    // 陈旧的 battle 节点或伪造结算。放行等于让战斗服点名销毁玩家的任意物品。
    constexpr uint32_t kNonBattleItem = 1;  // Item 表里 battle_usable = 0
    GiveItem(kNonBattleItem, 3);
    SetConsumed(kNonBattleItem, 3);

    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));

    EXPECT_EQ(Inventory().GetTotalItemCount(kNonBattleItem), 3u);  // 一件都没被扣
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
}

TEST_F(PlayerBattleSettlementItemTest, RepeatedApplyDoesNotDoubleConsumeOrDoubleDrop) {
    GiveItem(kSettlementStackItem, 5);
    SetConsumed(kSettlementStackItem, 2);
    SetGained(kSettlementDropItem, 3);

    ASSERT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement));
    // 同 (player, battle_id) 的重投由持久账本(BattleSettlementLedgerComp)挡掉:返回 true 并报告
    // alreadyApplied,道具不能再扣一次、也不能再发一次
    bool alreadyApplied = false;
    EXPECT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement, alreadyApplied));
    EXPECT_TRUE(alreadyApplied);

    EXPECT_EQ(Inventory().GetTotalItemCount(kSettlementStackItem), 3u);
    EXPECT_EQ(Inventory().GetTotalItemCount(kSettlementDropItem), 3u);
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
}

}  // namespace

// ---------------------------------------------------------------------------
// 2026-10-14 landing-recovery-sync-digest.md §7:待结算记录去掉到期删除 + 积压索引。
// 读写 pending 的 Lua 由 battle 与 scene 两端共用(settlement_outbox.h)。单测不连 Redis,
// 所以盯住脚本文本里的不变量:写入不带任何过期且必 ZADD 索引;销账必 ZREM,且只在删 pending 的分支里;
// 孤儿清理的 EXISTS 与 ZREM 在同一条脚本里。键位与 battle_room_manager.cpp / player_battle.cpp 的
// EVAL 参数顺序一一对应,改脚本键位时这里会先红。
// ---------------------------------------------------------------------------

namespace {

bool ScriptContains(const char* script, const std::string& needle) {
    return std::string(script).find(needle) != std::string::npos;
}

TEST(PendingSettlementContract, WritesCarryNoExpiry) {
    for (const char* script : {battle_settlement::kSetPendingSettlementScript,
                               battle_settlement::kStorePendingSettlementIfLockMatchScript}) {
        EXPECT_FALSE(ScriptContains(script, "'EX'")) << script;
        EXPECT_FALSE(ScriptContains(script, "'PX'")) << script;
        EXPECT_FALSE(ScriptContains(script, "EXPIRE")) << script;
        // KEEPTTL 会把旧版本写入留下的 7 天 TTL 原样保住,等于没去掉。
        EXPECT_FALSE(ScriptContains(script, "KEEPTTL")) << script;
    }
}

// KEYS[3]=索引,ARGV[3]=now_ms,ARGV[4]=player_id(DispatchSettlementDurably 的 EVAL 参数顺序)。
TEST(PendingSettlementContract, BattleWriteJoinsBacklogIndexInSameScript) {
    EXPECT_TRUE(ScriptContains(battle_settlement::kSetPendingSettlementScript,
                               "redis.call('ZADD', KEYS[3], ARGV[3], ARGV[4])"));
}

// KEYS[4]=索引(StorePendingSettlementIfLockMatch 的 EVAL 参数顺序)。锁不在 / 属于别的局时提前返回,
// 什么都不写,当然也不能进索引:ZADD 必须排在两处提前返回之后。
TEST(PendingSettlementContract, SceneStagingJoinsBacklogIndexOnlyWhenWritten) {
    const std::string script = battle_settlement::kStorePendingSettlementIfLockMatchScript;
    const auto zadd = script.find("redis.call('ZADD', KEYS[4], ARGV[3], ARGV[4])");
    ASSERT_NE(zadd, std::string::npos);
    ASSERT_NE(script.find("return 0"), std::string::npos);
    ASSERT_NE(script.find("return 2"), std::string::npos);
    EXPECT_GT(zadd, script.find("return 0"));
    EXPECT_GT(zadd, script.find("return 2"));
}

// ZREM 只在「pending:id == 本局」分支里:pending 已属于下一局时,索引成员代表的是下一局的记录,不能摘。
TEST(PendingSettlementContract, AckRemovesIndexMemberOnlyTogetherWithPending) {
    const std::string script = battle_settlement::kAckSettlementScript;
    const auto pendingBranch = script.find("if redis.call('GET', KEYS[2]) == ARGV[1] then");
    const auto lockBranch = script.find("if redis.call('GET', KEYS[3]) == ARGV[1] then");
    const auto zrem = script.find("redis.call('ZREM', KEYS[5], ARGV[2])");
    ASSERT_NE(pendingBranch, std::string::npos);
    ASSERT_NE(lockBranch, std::string::npos);
    ASSERT_NE(zrem, std::string::npos);
    EXPECT_GT(zrem, pendingBranch);
    EXPECT_LT(zrem, script.find(" end ", pendingBranch));
    EXPECT_LT(zrem, lockBranch);
    EXPECT_EQ(script.find("'ZREM'", zrem + 1), std::string::npos);
}

TEST(PendingSettlementContract, CorruptRecordCleanupAlsoRemovesIndexMember) {
    EXPECT_TRUE(ScriptContains(battle_settlement::kDeleteCorruptPendingSettlementScript,
                               "redis.call('DEL', KEYS[1], KEYS[2])"));
    EXPECT_TRUE(ScriptContains(battle_settlement::kDeleteCorruptPendingSettlementScript,
                               "redis.call('ZREM', KEYS[3], ARGV[1])"));
}

// 孤儿清理:pending:id 不在才 ZREM,判定与删除在同一条脚本里(否则两步之间写入的新一局会被误摘)。
TEST(PendingSettlementContract, OrphanSweepRemovesOnlyWhenRecordIsGone) {
    const std::string script = battle_settlement::kSweepPendingIndexOrphanScript;
    const auto exists = script.find("if redis.call('EXISTS', KEYS[2]) == 0 then");
    const auto zrem = script.find("redis.call('ZREM', KEYS[1], ARGV[1])");
    ASSERT_NE(exists, std::string::npos);
    ASSERT_NE(zrem, std::string::npos);
    EXPECT_LT(exists, zrem);
}

TEST(PendingSettlementContract, BacklogReadIsReadOnly) {
    for (const char* write : {"'ZREM'", "'ZADD'", "'DEL'", "'SET'"}) {
        EXPECT_FALSE(ScriptContains(battle_settlement::kReadPendingBacklogScript, write)) << write;
    }
    EXPECT_TRUE(ScriptContains(battle_settlement::kReadPendingBacklogScript, "'ZCARD'"));
}

// 索引键直接拼进 hiredis 的格式串(与 %llu 键格式同一种用法):不能含 '%'(会被当成格式说明符)、
// 不能含空白(hiredis 按空白切参数);也不能被按玩家的两个键格式撞上。
TEST(PendingSettlementContract, IndexKeyIsFormatSafeAndDistinctFromPerPlayerKeys) {
    const std::string index = battle_settlement::kPendingSettlementIndexKey;
    EXPECT_EQ(index, "battle:settlement:pending:index");
    EXPECT_EQ(index.find('%'), std::string::npos);
    EXPECT_EQ(index.find(' '), std::string::npos);
    EXPECT_NE(index.rfind("battle:settlement:pending:id:", 0), 0u);
    char key[128];
    for (const unsigned long long id : {1ull, 9876501ull, std::numeric_limits<unsigned long long>::max()}) {
        std::snprintf(key, sizeof(key), battle_settlement::kPendingSettlementKeyFmt, id);
        EXPECT_NE(index, key);
        std::snprintf(key, sizeof(key), battle_settlement::kPendingSettlementIdKeyFmt, id);
        EXPECT_NE(index, key);
    }
}

// 用尽日志的 reason 只看用尽那一轮能不能解析到位置。
TEST(SettlementOutboxUndelivered, ReasonFollowsTheExhaustingRound) {
    battle_settlement::RetryInput input;
    input.pendingRecordStillOurs = true;
    input.attemptsSoFar = 12;
    input.maxAttempts = 12;

    input.locationResolved = false;
    ASSERT_EQ(battle_settlement::ClassifyRetry(input), battle_settlement::RetryAction::kExhausted);
    EXPECT_EQ(battle_settlement::ClassifyUndelivered(input), battle_settlement::UndeliveredReason::kNoTarget);

    input.locationResolved = true;
    ASSERT_EQ(battle_settlement::ClassifyRetry(input), battle_settlement::RetryAction::kExhausted);
    EXPECT_EQ(battle_settlement::ClassifyUndelivered(input), battle_settlement::UndeliveredReason::kNotAcked);

    EXPECT_STREQ(battle_settlement::UndeliveredReasonName(battle_settlement::UndeliveredReason::kNoTarget), "no_target");
    EXPECT_STREQ(battle_settlement::UndeliveredReasonName(battle_settlement::UndeliveredReason::kNotAcked), "not_acked");
}

TEST(SettlementBacklog, OldestAgeIsZeroForEmptyIndexOrSkewedClock) {
    EXPECT_EQ(battle_settlement::PendingBacklogOldestAgeSec(0, 1'000'000), 0u);       // 索引为空
    EXPECT_EQ(battle_settlement::PendingBacklogOldestAgeSec(2'000, 1'000), 0u);       // 写入方墙钟快于报数方
    EXPECT_EQ(battle_settlement::PendingBacklogOldestAgeSec(1'000, 1'000), 0u);
    EXPECT_EQ(battle_settlement::PendingBacklogOldestAgeSec(1'000, 62'999), 61u);     // 向下取整到秒
    // 去掉 TTL 之后年龄可以远超原来的 7 天保留期,不能回绕。
    constexpr uint64_t kWrittenAtMs = 1'700'000'000'000ull;
    constexpr uint64_t kThirtyWeeksSec = 30ull * 7 * 24 * 3600;
    EXPECT_EQ(battle_settlement::PendingBacklogOldestAgeSec(kWrittenAtMs, kWrittenAtMs + kThirtyWeeksSec * 1000),
              kThirtyWeeksSec);
}

// 头部与轮转窗口交替;轮转窗口满批后移、不满批(扫到底)绕回 0。
TEST(SettlementBacklog, SweepAlternatesHeadAndRotatingWindowAndWraps) {
    constexpr uint32_t kBatch = 10;
    battle_settlement::PendingIndexSweepCursor cursor;
    EXPECT_EQ(battle_settlement::PendingIndexSweepStart(cursor), 0u);   // 第 0 轮:头部
    battle_settlement::AdvancePendingIndexSweep(cursor, 10, kBatch);
    EXPECT_EQ(battle_settlement::PendingIndexSweepStart(cursor), 0u);   // 第 1 轮:轮转窗口从 0 起
    battle_settlement::AdvancePendingIndexSweep(cursor, 10, kBatch);    // 满批:轮转窗口后移一批
    EXPECT_EQ(battle_settlement::PendingIndexSweepStart(cursor), 0u);   // 第 2 轮:头部
    battle_settlement::AdvancePendingIndexSweep(cursor, 10, kBatch);
    EXPECT_EQ(battle_settlement::PendingIndexSweepStart(cursor), 10u);  // 第 3 轮:轮转窗口 [10, 20)
    battle_settlement::AdvancePendingIndexSweep(cursor, 4, kBatch);     // 不满批:扫到底,绕回 0
    EXPECT_EQ(battle_settlement::PendingIndexSweepStart(cursor), 0u);   // 第 4 轮:头部
    battle_settlement::AdvancePendingIndexSweep(cursor, 10, kBatch);
    EXPECT_EQ(battle_settlement::PendingIndexSweepStart(cursor), 0u);   // 第 5 轮:轮转窗口已绕回
}

// 只查头部会卡死:打完不再上线的玩家让有效记录永久占住头部,后面的孤儿永远查不到。
// 轮转窗口必须在有限轮内走遍每个排名。
TEST(SettlementBacklog, RotatingWindowEventuallyCoversWholeIndex) {
    constexpr uint64_t kIndexSize = 95;
    constexpr uint32_t kBatch = 10;
    battle_settlement::PendingIndexSweepCursor cursor;
    std::vector<bool> covered(kIndexSize, false);
    for (int round = 0; round < 40; ++round) {
        const uint64_t start = battle_settlement::PendingIndexSweepStart(cursor);
        const uint64_t returned = start >= kIndexSize ? 0 : std::min<uint64_t>(kBatch, kIndexSize - start);
        for (uint64_t rank = start; rank < start + returned; ++rank) covered[rank] = true;
        battle_settlement::AdvancePendingIndexSweep(cursor, returned, kBatch);
    }
    for (uint64_t rank = 0; rank < kIndexSize; ++rank) {
        EXPECT_TRUE(covered[rank]) << "rank " << rank;
    }
}

TEST(SettlementBacklog, ZeroBatchLeavesCursorUntouched) {
    battle_settlement::PendingIndexSweepCursor cursor;
    cursor.round = 3;
    cursor.rotatingStart = 20;
    battle_settlement::AdvancePendingIndexSweep(cursor, 0, 0);
    EXPECT_EQ(cursor.round, 3u);
    EXPECT_EQ(cursor.rotatingStart, 20u);
}

TEST(SettlementBacklog, IndexMemberMustBePositiveDecimal) {
    uint64_t playerId = 0;
    EXPECT_TRUE(battle_settlement::ParsePendingIndexMember("9876501", playerId));
    EXPECT_EQ(playerId, 9876501u);
    EXPECT_TRUE(battle_settlement::ParsePendingIndexMember("18446744073709551615", playerId));
    EXPECT_EQ(playerId, std::numeric_limits<uint64_t>::max());
    for (const char* bad : {"", "0", "-1", "+1", " 1", "1 ", "12a", "0x10", "18446744073709551616"}) {
        playerId = 77;
        EXPECT_FALSE(battle_settlement::ParsePendingIndexMember(bad, playerId)) << '"' << bad << '"';
        EXPECT_EQ(playerId, 77u) << '"' << bad << '"';  // 失败不改出参
    }
}

// reaper 到期解冻之后的补读命中:ClearBattleFreeze 刚摘了 InBattleComp,ApplyPendingSettlement
// 照常入账一次、不凭空把玩家冻回去;同一份记录被再补读一次(下一轮 reaper / 登录钩子)由账本挡住。
TEST_F(PlayerBattleSettlementTest, PendingAppliedAfterFreezeExpiryPaysOnceWithoutRefreezing) {
    ASSERT_FALSE(tlsEcs.actorRegistry.any_of<InBattleComp>(player));
    for (int read = 0; read < 2; ++read) {
        PlayerBattleSettlementTestAccess::ApplyPending(player, settlement);
        EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u) << "read " << read;
        EXPECT_EQ(kills.size(), 2u) << "read " << read;
        EXPECT_FALSE(tlsEcs.actorRegistry.any_of<InBattleComp>(player)) << "read " << read;
    }
}

// 补读入口在 Redis 不可用时不应用、不改资产;2026-10-16 起记入登录复查(以前只打日志、等下次进场,
// 在线玩家一直拿不到奖励),由 reaper 节拍在 Redis 恢复后重读。
TEST_F(PlayerBattleSettlementTest, FreezeExpiryRecoveryWithoutRedisAppliesNothingAndSchedulesRecheck) {
    PlayerBattleSettlementTestAccess::RecoverAfterFreezeExpiry(player, playerId);
    EXPECT_TRUE(tlsEcs.actorRegistry.valid(player));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 0u);
    EXPECT_TRUE(kills.empty());
    EXPECT_EQ(gPersistCalls, 0);
    EXPECT_TRUE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
}

// ---- 2026-10-16:登录钩子「没问到」进复查(单测里没有 Redis 连接,RedisReady() 恒为 false)----
//
// 空应答与 Redis 不可用是同一类「没问到」;回调里按应答形状分流的判定由 routing_identity_test 的
// SettlementLoginHook.* 盯住,这里盯调用点:没问到时挂复查标记、不当结论、不被 reaper 节拍丢掉。

// 缺陷 3:以前只打一条 WARN「等下一次登录」,玩家一直在线就一直拿不到挂起结算,冻结也不重建。
TEST_F(PlayerBattleSettlementTest, EnterSceneWithoutRedisSchedulesLoginRecheck) {
    ASSERT_FALSE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
    PlayerBattleSystem::OnPlayerEnterScene(player, LOGIN_FIRST);
    EXPECT_TRUE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
    // 没问到就不是结论:不补应用、不凭空冻结。
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<InBattleComp>(player));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 0u);
    EXPECT_EQ(gPersistCalls, 0);
}

// reaper 节拍遇到 Redis 仍不可用:标记原样保留,等 Redis 恢复后的节拍再跑;重复节拍不叠加副作用。
TEST_F(PlayerBattleSettlementTest, LoginRecheckSurvivesReaperTicksWhileRedisIsDown) {
    PlayerBattleSystem::OnPlayerEnterScene(player, LOGIN_FIRST);
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
    for (int tick = 0; tick < 3; ++tick) {
        PlayerBattleSettlementTestAccess::RunLoginRechecks();
        EXPECT_TRUE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player)) << "tick " << tick;
    }
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<InBattleComp>(player));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 0u);
}

// 缺陷 4:挂起结算补应用之后按锁重建冻结,读不了锁时以前直接 return(当「无锁」)。
// 现在补应用照常只发一次,读锁那一步进复查。
TEST_F(PlayerBattleSettlementTest, PendingAppliedButLockUnreadableSchedulesRecheck) {
    ASSERT_FALSE(tlsEcs.actorRegistry.any_of<InBattleComp>(player));
    PlayerBattleSettlementTestAccess::ApplyPending(player, settlement);
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
    EXPECT_TRUE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
    // 复查重跑时同一份记录再补应用一次由账本挡住(PendingAppliedAfterFreezeExpiryPaysOnceWithoutRefreezing 同理)。
    PlayerBattleSettlementTestAccess::ApplyPending(player, settlement);
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 12u);
}

// 退出中的实体视同离线:节拍摘掉标记、不重跑(不论 Redis 是否可用),挂起结算留给下次进场。
TEST_F(PlayerBattleSettlementTest, ExitingPlayerLoginRecheckIsDroppedWithoutRerun) {
    PlayerBattleSystem::OnPlayerEnterScene(player, LOGIN_FIRST);
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
    tlsEcs.actorRegistry.emplace<UnregisterPlayer>(player);
    PlayerBattleSettlementTestAccess::RunLoginRechecks();
    EXPECT_FALSE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
    EXPECT_EQ(CurrencySystem::GetBalance(player, kCurrencyGold), 0u);
}

// 跨 zone 交接中:标记保留、本轮不跑 —— 重跑会在即将销毁的源实体上按锁重建冻结并推重连提示。
// 交接完成随实体销毁;交接撤销后下一个节拍再跑。
TEST_F(PlayerBattleSettlementTest, TravelHandoffKeepsLoginRecheckForLater) {
    PlayerBattleSystem::OnPlayerEnterScene(player, LOGIN_FIRST);
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
    tlsEcs.actorRegistry.emplace<PlayerTravelHandoffComp>(player);
    PlayerBattleSettlementTestAccess::RunLoginRechecks();
    EXPECT_TRUE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<InBattleComp>(player));
}

// 标记只挂在实体上:实体销毁即随之消失(下次进场登录钩子会重跑),不会留下指向已销毁实体的复查。
TEST_F(PlayerBattleSettlementTest, LoginRecheckDiesWithTheEntity) {
    PlayerBattleSystem::OnPlayerEnterScene(player, LOGIN_FIRST);
    ASSERT_TRUE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
    tlsEcs.actorRegistry.destroy(player);
    EXPECT_FALSE(PlayerBattleSettlementTestAccess::LoginRecheckPending(player));
    PlayerBattleSettlementTestAccess::RunLoginRechecks();  // 无标记可跑,也不碰已销毁实体
    player = entt::null;  // TearDown 不再 destroy
}

}  // namespace
