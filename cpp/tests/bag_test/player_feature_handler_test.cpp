#include <gtest/gtest.h>
#include "engine/core/type_define/player_guid_comp.h"
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "../../nodes/scene/handler/rpc/player/player_activity_handler.h"
#include "../../nodes/scene/handler/rpc/player/player_attribute_handler.h"
#include "../../nodes/scene/handler/rpc/player/player_bag_handler.h"
#include "../../nodes/scene/handler/rpc/player/player_mission_handler.h"
#include "../../nodes/scene/handler/rpc/player/player_pet_handler.h"
#include "proto/battle/battle_data.pb.h"
#include "proto/common/component/actor_attribute_state_comp.pb.h"
#include "proto/common/component/actor_combat_state_comp.pb.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/player_attribute_comp.pb.h"
#include "proto/common/component/player_pet_comp.pb.h"
#include "proto/scene/player_pet.pb.h"
#include "services/scene/actor/action_state/constants/actor_state.h"
#include "services/scene/actor/action_state/system/actor_action_state.h"
#include "services/scene/actor/attribute/comp/actor_attribute_comp.h"
#include "services/scene/actor/attribute/system/actor_attribute_calculator.h"
#include "services/scene/player/system/player_pet.h"
#include "table/code/actoractionstate_table.h"
#include "table/code/attributedimension_table.h"
#include "table/code/attributepool_table.h"
#include "table/code/pet_table.h"
#include "table/code/petrule_table.h"
#include "modules/bag/comp/player_bags_comp.h"
#include "modules/currency/comp/player_currency_comp.h"
#include "modules/currency/constants/currency.h"
#include "modules/id_segment/guid_segment_registry.h"
#include "modules/mission/comp/mission_comp.h"
#include "modules/mission/system/mission.h"
#include "modules/condition/condition_type.h"
#include "modules/mission/mission_events.h"
#include "services/scene/player/comp/player_frozen_comp.h"
#include "services/scene/player/system/player_mission.h"
#include "table/code/condition_table.h"
#include "table/code/dungeon_table.h"
#include "table/code/item_table.h"
#include "table/code/monster_table.h"
#include "table/code/reward_table.h"
#include "table/proto/tip/mission_error_tip.pb.h"
#include "table/proto/tip/reward_error_tip.pb.h"
#include "rpc/service_metadata/player_mission_service_metadata.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "thread_context/ecs_context.h"

namespace {
class TestBagService : public SceneBagClientPlayer {};
class TestMissionService : public SceneMissionClientPlayer {};
class TestActivityService : public SceneActivityClientPlayer {};
class TestAttributeService : public SceneAttributeClientPlayer {};
class TestPetService : public ScenePetClientPlayer {};

TEST(PlayerFeatureHandlerTest, AttributePanelMissingPersistentDataIsWrittenToResponse) {
    const auto player = tlsEcs.actorRegistry.create();
    SceneAttributeClientPlayerHandler handler(std::make_unique<TestAttributeService>());
    GetAttributePanelRequest request;
    GetAttributePanelResponse response;
    response.mutable_panel()->set_level(30);

    handler.CallMethod(SceneAttributeClientPlayer::descriptor()->method(0), player, &request, &response);

    EXPECT_EQ(kServiceUnavailable, response.error_message().id());
    EXPECT_FALSE(response.has_panel());
    EXPECT_FALSE(tlsEcs.actorRegistry.all_of<PlayerAttributeComp>(player));
    tlsEcs.actorRegistry.destroy(player);
}

TEST(PlayerFeatureHandlerTest, PetListMissingPersistentDataIsWrittenToResponse) {
    const auto player = tlsEcs.actorRegistry.create();
    tlsEcs.actorRegistry.emplace<LevelComp>(player);
    ScenePetClientPlayerHandler handler(std::make_unique<TestPetService>());
    GetPetListRequest request;
    GetPetListResponse response;
    response.mutable_pets()->set_active_pet_id(901);

    handler.CallMethod(ScenePetClientPlayer::descriptor()->method(0), player, &request, &response);

    EXPECT_EQ(kServiceUnavailable, response.error_message().id());
    EXPECT_FALSE(response.has_pets());
    EXPECT_FALSE(tlsEcs.actorRegistry.all_of<PlayerPetComp>(player));
    tlsEcs.actorRegistry.destroy(player);
}

TEST(PlayerFeatureHandlerTest, BagRejectionIsWrittenToResponseAndDoesNotLeak) {
    const auto player = tlsEcs.actorRegistry.create();
    tlsEcs.actorRegistry.emplace<PlayerBagsComp>(player);
    SceneBagClientPlayerHandler handler(std::make_unique<TestBagService>());
    GetBagRequest request;
    request.set_bag_type(4);
    GetBagResponse response;
    handler.CallMethod(SceneBagClientPlayer::descriptor()->method(0), player, &request, &response);
    EXPECT_EQ(kInvalidParameter, response.error_message().id());
    EXPECT_FALSE(response.has_bag());
    request.set_bag_type(0);
    response.Clear();
    handler.CallMethod(SceneBagClientPlayer::descriptor()->method(0), player, &request, &response);
    EXPECT_EQ(0u, response.error_message().id());
    EXPECT_TRUE(response.has_bag());
    tlsEcs.actorRegistry.destroy(player);
}

TEST(PlayerFeatureHandlerTest, MissionInvalidEntityIsWrittenToResponse) {
    SceneMissionClientPlayerHandler handler(std::make_unique<TestMissionService>());
    GetMissionListRequest request;
    GetMissionListResponse response;
    handler.CallMethod(SceneMissionClientPlayer::descriptor()->method(0), entt::null, &request, &response);
    EXPECT_EQ(kThisEntityIsInvalid, response.error_message().id());
    EXPECT_TRUE(response.missions().empty());
}

TEST(PlayerFeatureHandlerTest, ActivityInvalidEntityIsWrittenToResponse) {
    SceneActivityClientPlayerHandler handler(std::make_unique<TestActivityService>());
    GetActivityListRequest request;
    GetActivityListResponse response;
    handler.CallMethod(SceneActivityClientPlayer::descriptor()->method(0), entt::null, &request, &response);
    EXPECT_EQ(kThisEntityIsInvalid, response.error_message().id());
    EXPECT_TRUE(response.activities().empty());
}
class MissionActionHandlerTest : public testing::Test {
protected:
    static bool ArmItemSegment(uint64_t lo, uint64_t hi) {
        auto& segment = tlsGuidSegmentRegistry.Get(GuidKind::kItem);
        segment.Reset();
        GuidSegmentClient::Options options;
        options.kindName = "item";
        options.initialStep = 100000;
        if (!segment.Enable(options, [](const std::string&, uint32_t) { return true; },
            [](GuidSegmentClient::TimerKind, double, std::function<void()>) {}, [] { return 0.0; })) return false;
        segment.Warm();
        segment.OnResponse(0, lo, hi);
        return segment.IsReady();
    }
    void SetUp() override {
        MissionTableManager::Instance().Load();
        ConditionTableManager::Instance().Load();
        RewardTableManager::Instance().Load();
        ItemTableManager::Instance().Load();
        DungeonTableManager::Instance().Load();
        MonsterTableManager::Instance().Load();
        ASSERT_TRUE(ArmItemSegment(700000, 800000));
        player = tlsEcs.actorRegistry.create();
        tlsEcs.actorRegistry.emplace<PlayerGuidComp>(player, Guid{454545});
        auto& bags = tlsEcs.actorRegistry.emplace<PlayerBagsComp>(player);
        for (auto& bag : bags.bags) bag.SetPlayerGuid(Guid{454545});
        ASSERT_EQ(kSuccess, PlayerMissionSystem::Initialize(player));
    }
    void TearDown() override {
        tlsEcs.dispatcher.clear<AcceptMissionEvent>();
        tlsEcs.dispatcher.clear<ConditionEvent>();
        tlsEcs.dispatcher.clear<OnAcceptedMissionEvent>();
        tlsEcs.dispatcher.clear<OnMissionAwardEvent>();
        if (tlsEcs.actorRegistry.valid(player)) tlsEcs.actorRegistry.destroy(player);
        EXPECT_TRUE(ArmItemSegment(1, uint64_t{1} << 54));
    }
    MissionsComp& Missions() { return tlsEcs.actorRegistry.get<MissionsContainerComp>(player).map.at(0); }
    Bag& Inventory() { return tlsEcs.actorRegistry.get<PlayerBagsComp>(player).bags[kInventory]; }
    GetMissionListResponse Act(bool claim, uint32_t id = 12, uint32_t scope = 0) {
        MissionActionRequest request;
        request.set_scope(scope);
        request.set_mission_id(id);
        GetMissionListResponse response;
        handler.CallMethod(claim ? SceneMissionClientPlayerClaimMissionRewardMethod :
                                   SceneMissionClientPlayerAcceptMissionMethod, player, &request, &response);
        return response;
    }
    GetMissionListResponse Read() {
        GetMissionListRequest request;
        GetMissionListResponse response;
        handler.CallMethod(SceneMissionClientPlayerGetMissionListMethod, player, &request, &response);
        return response;
    }
    static const PlayerMissionInfo* Find(const GetMissionListResponse& response, uint32_t id = 12) {
        for (const auto& info : response.missions())
            if (info.scope() == 0 && info.mission_id() == id) return &info;
        return nullptr;
    }
    void CompleteManualMission() {
        ASSERT_EQ(0u, Act(false).error_message().id());
        ConditionEvent kill;
        kill.player = player;
        kill.conditionType = static_cast<uint32_t>(eConditionType::kConditionKillMonster);
        kill.conditionIds.push_back(1);
        kill.amount = 1;
        MissionSystem::HandleConditionEvent(kill, Missions(), MissionConfig::GetSingleton());
        ASSERT_TRUE(Missions().IsClaimable(12));
    }
    uint32_t ReadBagCount() {
        SceneBagClientPlayerHandler bagHandler(std::make_unique<TestBagService>());
        GetBagRequest request;
        request.set_bag_type(kInventory);
        GetBagResponse response;
        bagHandler.CallMethod(SceneBagClientPlayer::descriptor()->method(0), player, &request, &response);
        EXPECT_EQ(0u, response.error_message().id());
        uint32_t count = 0;
        for (const auto& item : response.bag().items()) count += item.count();
        return count;
    }
    entt::entity player{entt::null};
    SceneMissionClientPlayerHandler handler{std::make_unique<TestMissionService>()};
};

TEST_F(MissionActionHandlerTest, ActionRpcIdsAndInvalidEntityErrorsUseGeneratedDispatch) {
    EXPECT_EQ(194u, SceneMissionClientPlayerAcceptMissionMessageId);
    EXPECT_EQ(195u, SceneMissionClientPlayerClaimMissionRewardMessageId);
    MissionActionRequest request;
    request.set_mission_id(12);
    for (const auto* method : {SceneMissionClientPlayerAcceptMissionMethod, SceneMissionClientPlayerClaimMissionRewardMethod}) {
        GetMissionListResponse response;
        handler.CallMethod(method, entt::null, &request, &response);
        EXPECT_EQ(kThisEntityIsInvalid, response.error_message().id());
        EXPECT_TRUE(response.missions().empty());
        EXPECT_FALSE(response.state_persistent());
    }
}

TEST_F(MissionActionHandlerTest, AcceptReturnsActiveSnapshotAndDuplicateFailureDoesNotLeakIntoRead) {
    const auto accepted = Act(false);
    ASSERT_EQ(0u, accepted.error_message().id());
    EXPECT_TRUE(accepted.state_persistent());
    const auto* row = Find(accepted);
    ASSERT_NE(nullptr, row);
    EXPECT_EQ(PLAYER_MISSION_ACTIVE, row->status());
    EXPECT_FALSE(row->can_accept());
    EXPECT_FALSE(row->can_claim());
    const auto duplicate = Act(false);
    EXPECT_EQ(kMissionIdRepeated, duplicate.error_message().id());
    EXPECT_TRUE(duplicate.missions().empty());
    const auto snapshot = Read();
    EXPECT_EQ(0u, snapshot.error_message().id());
    ASSERT_NE(nullptr, Find(snapshot));
    EXPECT_EQ(PLAYER_MISSION_ACTIVE, Find(snapshot)->status());
    EXPECT_EQ(0u, ReadBagCount());
}

TEST_F(MissionActionHandlerTest, PrematureOrAlternateScopeClaimCannotGrantReward) {
    EXPECT_EQ(kMissionIdNotInRewardList, Act(true).error_message().id());
    EXPECT_EQ(kInvalidParameter, Act(false, 12, 1).error_message().id());
    CompleteManualMission();
    EXPECT_EQ(kInvalidParameter, Act(true, 12, 1).error_message().id());
    EXPECT_TRUE(Missions().IsClaimable(12));
    EXPECT_EQ(0u, ReadBagCount());
}

TEST_F(MissionActionHandlerTest, ClaimReturnsCompletedSnapshotAndBagReadShowsOneReward) {
    CompleteManualMission();
    const auto before = Read();
    ASSERT_NE(nullptr, Find(before));
    EXPECT_EQ(PLAYER_MISSION_CLAIMABLE, Find(before)->status());
    EXPECT_TRUE(Find(before)->can_claim());
    const auto claimed = Act(true);
    ASSERT_EQ(0u, claimed.error_message().id());
    ASSERT_NE(nullptr, Find(claimed));
    EXPECT_EQ(PLAYER_MISSION_COMPLETED, Find(claimed)->status());
    EXPECT_FALSE(Find(claimed)->can_claim());
    EXPECT_TRUE(claimed.state_persistent());
    EXPECT_EQ(4u, ReadBagCount());
    const auto duplicate = Act(true);
    EXPECT_EQ(kRewardAlreadyClaimed, duplicate.error_message().id());
    EXPECT_TRUE(duplicate.missions().empty());
    EXPECT_EQ(4u, ReadBagCount());
}

TEST_F(MissionActionHandlerTest, FullBagErrorRetainsClaimAndRetryReturnsFreshSnapshot) {
    CompleteManualMission();
    Inventory().SetCapacityForRestore(3);
    const auto failed = Act(true);
    EXPECT_NE(0u, failed.error_message().id());
    EXPECT_TRUE(failed.missions().empty());
    EXPECT_TRUE(Missions().IsClaimable(12));
    EXPECT_EQ(0u, ReadBagCount());
    const auto beforeRetry = Read();
    ASSERT_NE(nullptr, Find(beforeRetry));
    EXPECT_TRUE(Find(beforeRetry)->can_claim());
    Inventory().SetCapacityForRestore(4);
    const auto retried = Act(true);
    ASSERT_EQ(0u, retried.error_message().id());
    ASSERT_NE(nullptr, Find(retried));
    EXPECT_EQ(PLAYER_MISSION_COMPLETED, Find(retried)->status());
    EXPECT_EQ(4u, ReadBagCount());
}

TEST_F(MissionActionHandlerTest, FrozenActionsPreserveStateAndReadOnlySnapshotDisablesClaim) {
    CompleteManualMission();
    tlsEcs.actorRegistry.emplace<PlayerFrozenComp>(player);
    EXPECT_EQ(kInvalidParameter, Act(true).error_message().id());
    EXPECT_EQ(kInvalidParameter, Act(false, 13).error_message().id());
    const auto frozen = Read();
    ASSERT_NE(nullptr, Find(frozen));
    EXPECT_EQ(PLAYER_MISSION_CLAIMABLE, Find(frozen)->status());
    EXPECT_FALSE(Find(frozen)->can_claim());
    EXPECT_TRUE(Missions().IsClaimable(12));
    EXPECT_FALSE(Missions().IsAccepted(13));
    EXPECT_EQ(0u, ReadBagCount());
}

// ═════════════════════════════════════════════════════════════════════════
// 三类组件分别验证:派生 MoveSpeedComp / CombatStateFlagsComp 在 actor 创建时挂,
// 持久 PlayerPetComp 由加载链路挂;按需 ActorStateComp 在第一次状态写入时创建。
// 初始化链不编进本工程,派生与持久组件缺失场景由夹具显式构造,按需状态调用真实写入口。
// ═════════════════════════════════════════════════════════════════════════

// 用例自己建的实体统一登记、TearDown 销毁,不把组件留给后面的用例。
class ActorEntityFixture : public testing::Test {
protected:
    void TearDown() override {
        for (const auto entity : entities) {
            if (tlsEcs.actorRegistry.valid(entity)) tlsEcs.actorRegistry.destroy(entity);
        }
    }
    entt::entity NewEntity() {
        const auto entity = tlsEcs.actorRegistry.create();
        entities.push_back(entity);
        return entity;
    }
    template <typename Comp>
    bool Has(entt::entity entity) const { return tlsEcs.actorRegistry.all_of<Comp>(entity); }

    std::vector<entt::entity> entities;
};

// ── 属性脏标记(F035):按需状态,标脏时挂上、消费后摘掉 ─────────────────────

class ActorAttributeCalculatorTest : public ActorEntityFixture {};

// 修复前 Update 只清位不摘组件:标过一次脏的 actor 永远留在 view 里,每 tick 白遍历一遍。
TEST_F(ActorAttributeCalculatorTest, UpdateRemovesTheDirtyFlagComponentOnceConsumed) {
    const auto actor = NewEntity();
    tlsEcs.actorRegistry.emplace<MoveSpeedComp>(actor);
    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(actor, kMoveSpeed);
    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(actor, kHealth);
    ASSERT_TRUE(Has<AttributeDirtyFlagsComp>(actor));
    {
        const auto &bits = tlsEcs.actorRegistry.get<AttributeDirtyFlagsComp>(actor).attributeBits;
        EXPECT_TRUE(bits.test(kMoveSpeed)) << "marks within one tick must merge, not replace each other";
        EXPECT_TRUE(bits.test(kHealth));
    }

    ActorAttributeCalculatorSystem::Update();
    EXPECT_FALSE(Has<AttributeDirtyFlagsComp>(actor));
}

// 跨 zone 冻结中的玩家不重算,标记原样留到冻结解除(view 的 exclude 口径不变),解冻后消费即摘。
// 修复前解冻那次 Update 只清位不摘组件,而且没有 buff 列表时直接 return,移速停在 2.0。
TEST_F(ActorAttributeCalculatorTest, FrozenPlayerKeepsItsDirtyFlagsUntilTheFreezeEnds) {
    const auto actor = NewEntity();
    tlsEcs.actorRegistry.emplace<MoveSpeedComp>(actor).moveSpeed = 2.0;
    tlsEcs.actorRegistry.emplace<PlayerFrozenComp>(actor);
    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(actor, kMoveSpeed);

    ActorAttributeCalculatorSystem::Update();
    ASSERT_TRUE(Has<AttributeDirtyFlagsComp>(actor));
    EXPECT_DOUBLE_EQ(2.0, tlsEcs.actorRegistry.get<MoveSpeedComp>(actor).moveSpeed);

    tlsEcs.actorRegistry.remove<PlayerFrozenComp>(actor);
    ActorAttributeCalculatorSystem::Update();
    EXPECT_FALSE(Has<AttributeDirtyFlagsComp>(actor));
    EXPECT_DOUBLE_EQ(0.0, tlsEcs.actorRegistry.get<MoveSpeedComp>(actor).moveSpeed);
}

// 没走 actor 初始化的实体:重算函数只取不建,缺组件报错跳过;标记照样消费,不每 tick 重报。
// 修复前 UpdateMoveSpeed / ResetCombatStateFlags 用 get_or_emplace,会把这两个组件补出来。
TEST_F(ActorAttributeCalculatorTest, RecalculationDoesNotCreateMissingRequiredComponents) {
    const auto actor = NewEntity();
    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(actor, kMoveSpeed);
    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(actor, kCombatState);

    ActorAttributeCalculatorSystem::Update();
    EXPECT_FALSE(Has<MoveSpeedComp>(actor));
    EXPECT_FALSE(Has<CombatStateFlagsComp>(actor));
    EXPECT_FALSE(Has<AttributeDirtyFlagsComp>(actor));
}

// 身上没有 buff 列表(按需组件)= 没有移速加成:移速回到 0。
// 修复前这里直接 return,上一次算出的加成一直留在 MoveSpeedComp 里。
TEST_F(ActorAttributeCalculatorTest, MoveSpeedFallsBackToZeroWhenNoBuffListIsLeft) {
    const auto actor = NewEntity();
    tlsEcs.actorRegistry.emplace<MoveSpeedComp>(actor).moveSpeed = 3.5;
    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(actor, kMoveSpeed);

    ActorAttributeCalculatorSystem::Update();
    EXPECT_DOUBLE_EQ(0.0, tlsEcs.actorRegistry.get<MoveSpeedComp>(actor).moveSpeed);
}

// 走过初始化的 actor:同步投影照旧等于当前激活的战斗状态(键存在即激活)。修复前后都应成立。
TEST_F(ActorAttributeCalculatorTest, CombatStateFlagsMirrorTheActiveCombatStates) {
    constexpr uint32_t kSomeCombatState = 2;
    const auto actor = NewEntity();
    tlsEcs.actorRegistry.emplace<CombatStateFlagsComp>(actor);
    auto &collection = tlsEcs.actorRegistry.emplace<CombatStateCollectionComp>(actor);
    (*(*collection.mutable_states())[kSomeCombatState].mutable_sources())[uint64_t{42}] = false;
    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(actor, kCombatState);

    ActorAttributeCalculatorSystem::Update();
    const auto &flags = tlsEcs.actorRegistry.get<CombatStateFlagsComp>(actor).state_flags();
    ASSERT_EQ(1u, flags.size());
    ASSERT_TRUE(flags.contains(kSomeCombatState));
    EXPECT_TRUE(flags.at(kSomeCombatState));
}

// 越界的属性位不能碰组件:修复前先 get_or_emplace 出标记组件,再由 bitset::set 抛 out_of_range。
TEST_F(ActorAttributeCalculatorTest, MarkIgnoresAnOutOfRangeAttributeBit) {
    const auto actor = NewEntity();
    EXPECT_NO_THROW(ActorAttributeCalculatorSystem::MarkAttributeForUpdate(actor, kAttributeCalculatorMax));
    EXPECT_FALSE(Has<AttributeDirtyFlagsComp>(actor));
}

// ── actor 行为状态(F034):空缺是无激活状态,AddState 是唯一延迟创建入口 ───────────

class ActorActionStateTest : public ActorEntityFixture {
protected:
    void SetUp() override {
        ActorActionStateTableManager::Instance().Load();
        // 前置:「释放技能」这一行必须在表里,否则查表先失败,下面的用例就验不到组件这一步。
        ASSERT_NE(nullptr, ActorActionStateTableManager::Instance().FindByIdSilent(kActorActionUseSkill).first)
            << "ActorActionState row " << kActorActionUseSkill << " is missing: run the data table exporter first";
    }
};

// 只读与幂等移除不创建空表;活实体缺表是正常空状态,不是初始化失败。
TEST_F(ActorActionStateTest, MissingStateIsEmptyAndReadOrRemovalCreatesNothing) {
    const auto actor = NewEntity();
    EXPECT_EQ(kSuccess,
              ActorActionStateSystem::CanExecuteActionWithoutStateChange(actor, kActorActionUseSkill));
    EXPECT_FALSE(ActorActionStateSystem::HasState(actor, kActorStateCombat));
    EXPECT_EQ(kSuccess, ActorActionStateSystem::RemoveState(actor, kActorStateCombat));
    EXPECT_FALSE(Has<ActorStateComp>(actor));
}

// 修复前把空表当成必需组件,首次合法状态写入会被拒绝。
TEST_F(ActorActionStateTest, FirstActionCreatesTheStateAndRepeatedAddsPreserveIt) {
    const auto actor = NewEntity();
    EXPECT_EQ(kSuccess, ActorActionStateSystem::TryPerformAction(actor, kActorActionUseSkill, kActorStateCombat));
    EXPECT_TRUE(ActorActionStateSystem::HasState(actor, kActorStateCombat));
    EXPECT_EQ(kSuccess, ActorActionStateSystem::AddState(actor, kActorStateCombat));
    EXPECT_EQ(1u, tlsEcs.actorRegistry.get<ActorStateComp>(actor).state_list().size());
    EXPECT_EQ(kSuccess, ActorActionStateSystem::CanExecuteActionWithoutStateChange(actor, kActorActionUseSkill));
}

TEST_F(ActorActionStateTest, InvalidFirstStateCreatesNothing) {
    const auto actor = NewEntity();
    EXPECT_EQ(kInvalidParameter, ActorActionStateSystem::AddState(actor, kActorStateActorStateMax));
    EXPECT_EQ(kInvalidParameter,
              ActorActionStateSystem::TryPerformAction(actor, kActorActionUseSkill, kActorStateActorStateMax));
    EXPECT_FALSE(Has<ActorStateComp>(actor));
}

TEST_F(ActorActionStateTest, DestroyedEntityIsRejectedInsteadOfBeingTreatedAsEmpty) {
    const auto actor = NewEntity();
    tlsEcs.actorRegistry.destroy(actor);

    EXPECT_EQ(kThisEntityIsInvalid, ActorActionStateSystem::AddState(actor, kActorStateCombat));
    EXPECT_EQ(kThisEntityIsInvalid,
              ActorActionStateSystem::TryPerformAction(actor, kActorActionUseSkill, kActorStateCombat));
    EXPECT_EQ(kThisEntityIsInvalid,
              ActorActionStateSystem::CanExecuteActionWithoutStateChange(actor, kActorActionUseSkill));
    EXPECT_EQ(kThisEntityIsInvalid, ActorActionStateSystem::RemoveState(actor, kActorStateCombat));
    EXPECT_FALSE(ActorActionStateSystem::HasState(actor, kActorStateCombat));
    EXPECT_FALSE(Has<ActorStateComp>(actor));
}

// 非法的目标状态在任何状态改动之前被拒(校验前置),已有状态原样保留。
// 覆盖范围如实说明:现行 ActorActionState 表里没有任何 kActionStateInterrupt 行,打断分支走不到,
// 所以本用例只钉住返回码与「状态不变」,守不住「successState 校验排在打断之前」这一处顺序 ——
// 修复前后它都是绿的。要守住顺序,需要表里有打断行,或给表管理器加注入测试数据的接缝。
TEST_F(ActorActionStateTest, InvalidSuccessStateIsRejectedBeforeAnyStateChange) {
    const auto actor = NewEntity();
    auto &states = tlsEcs.actorRegistry.emplace<ActorStateComp>(actor);
    (*states.mutable_state_list())[static_cast<uint32_t>(kActorStateMounted)] = true;
    EXPECT_EQ(kInvalidParameter,
              ActorActionStateSystem::TryPerformAction(actor, kActorActionUseSkill, kActorStateActorStateMax));
    EXPECT_TRUE(ActorActionStateSystem::HasState(actor, kActorStateMounted));
    EXPECT_EQ(1u, tlsEcs.actorRegistry.get<ActorStateComp>(actor).state_list().size());
}

// ── 宝宝组件(F074):PlayerPetComp 由加载链路挂上,PetSystem 所有入口只取不建 ─────────

class PetComponentOwnershipTest : public ActorEntityFixture {
protected:
    void SetUp() override {
        PetTableManager::Instance().Load();
        PetRuleTableManager::Instance().Load();
        AttributePoolTableManager::Instance().Load();
        AttributeDimensionTableManager::Instance().Load();
        const auto &rows = PetTableManager::Instance().FindAll();
        ASSERT_GT(rows.data_size(), 0) << "Pet table is empty: run the data table exporter first";
        petTableId = rows.data(0).id();
        // 前置:宝宝池存在,否则 Allocate / Reset / GrantPet 会先因表缺失返回,验不到组件这一步。
        ASSERT_FALSE(AttributePoolTableManager::Instance().GetByOwnerType(PetSystem::kPoolOwnerPet).empty());
        // 没走加载链路的玩家:身份在,宝宝组件不在。
        player = NewEntity();
        tlsEcs.actorRegistry.emplace<PlayerGuidComp>(player, Guid{565656});
    }

    entt::entity player{entt::null};
    uint32_t petTableId{0};
};

// 只读 / 重算 / 战斗接缝:缺组件时不带宝宝、不推列表、不补建组件;列表接口报故障码,
// 让 handler 回错误而不是把空列表当成功发给客户端。
// 修复前 EnsureComp 会在第一个调用(BuildList)上就补出一个空的 PlayerPetComp(且 BuildList 没有返回值)。
TEST_F(PetComponentOwnershipTest, ReadAndRecalculateEntriesCreateNothing) {
    PetListInfo list;
    list.set_active_pet_id(99);
    EXPECT_EQ(kServiceUnavailable, PetSystem::BuildList(player, list));
    EXPECT_EQ(0u, list.active_pet_id());
    EXPECT_EQ(0, list.pets_size());

    ::BattlePetSnapshot snapshot;
    EXPECT_FALSE(PetSystem::BuildBattleSnapshot(player, snapshot));
    ::BattlePetSettlementData settlement;
    settlement.set_pet_id(1);
    PetSystem::ApplyBattleSettlement(player, settlement);
    PetSystem::InitializeOnLoad(player);
    PetSystem::RecalculateAll(player, PetSystem::RecalcReason::kLevelChanged);
    PetSystem::PushList(player);

    EXPECT_FALSE(Has<PlayerPetComp>(player));
}

// 写入口与交易原语:一律在扣费 / 铸号 / 改动之前拒绝,返回「缺必需组件」的服务端故障码
// kServiceUnavailable(与 player_mission.cpp / skill.cpp 同口径;实体无效才是 kEntityIsNull)。
// 修复前它们都先补出空组件再往下走:Summon 答「没有这只宝宝」,GrantPet 真的发出一只,
// RestorePetFromSnapshot 把交易快照塞进一个凭空造出来的组件并答成功。
TEST_F(PetComponentOwnershipTest, WriteEntriesAndTradePrimitivesRejectWithoutTheComponent) {
    EXPECT_EQ(kServiceUnavailable, PetSystem::Summon(player, 1));
    EXPECT_EQ(kServiceUnavailable, PetSystem::Recall(player));
    EXPECT_EQ(kServiceUnavailable, PetSystem::Allocate(player, 1, std::map<uint32_t, uint32_t>{}));
    EXPECT_EQ(kServiceUnavailable, PetSystem::Reset(player, 1));
    std::map<uint32_t, uint32_t> suggested{{1u, 1u}};
    EXPECT_EQ(kServiceUnavailable, PetSystem::AutoAllocate(player, 1, suggested));
    EXPECT_TRUE(suggested.empty());
    EXPECT_EQ(kServiceUnavailable, PetSystem::Rename(player, 1, "小白"));
    uint64_t petId = 77;
    EXPECT_EQ(kServiceUnavailable, PetSystem::GrantPet(player, petTableId, petId));
    EXPECT_EQ(0u, petId);

    PetInstance taken;
    taken.set_pet_id(5);
    EXPECT_EQ(kServiceUnavailable, PetSystem::RemovePetForTrade(player, 1, taken));
    EXPECT_EQ(0u, taken.pet_id()) << "a refused take-out must leave the snapshot cleared";
    PetInstance snapshot;
    snapshot.set_pet_id(900001);
    snapshot.set_pet_table_id(petTableId);
    EXPECT_EQ(kServiceUnavailable, PetSystem::RestorePetFromSnapshot(player, snapshot));

    EXPECT_FALSE(Has<PlayerPetComp>(player));
}

// 列表接口的两种失败要分得开:实体无效 → kEntityIsNull;加载链路挂好组件后 → kSuccess(空宝宝也是成功)。
TEST_F(PetComponentOwnershipTest, BuildListReportsInvalidEntityAndSucceedsOnceLoaded) {
    PetListInfo list;
    EXPECT_EQ(kEntityIsNull, PetSystem::BuildList(entt::null, list));

    tlsEcs.actorRegistry.emplace<PlayerPetComp>(player);
    EXPECT_EQ(kSuccess, PetSystem::BuildList(player, list));
    EXPECT_EQ(0, list.pets_size());
}

// 已加载的宝宝仍是持久数据;缺少主人等级时不能拿默认 1 级覆盖它。
class PetOwnerLevelTest : public PetComponentOwnershipTest {
protected:
    void SetUp() override {
        PetComponentOwnershipTest::SetUp();
        if (HasFatalFailure()) return;
        const auto* row = PetTableManager::Instance().FindById(petTableId).first;
        ASSERT_NE(nullptr, row);
        ASSERT_GE(row->level_cap(), kSavedLevel);
        const auto& pools = AttributePoolTableManager::Instance().GetByOwnerType(PetSystem::kPoolOwnerPet);
        ASSERT_FALSE(pools.empty());
        pool = std::cref(*pools.front());
        const auto& dimensions = AttributeDimensionTableManager::Instance().GetByPoolId(pool->get().id());
        ASSERT_FALSE(dimensions.empty());
        dimensionId = dimensions.front()->id();
        ASSERT_LE(pool->get().unlock_level(), kSavedLevel);
        ASSERT_TRUE(pool->get().dimension_cap() == 0 || pool->get().dimension_cap() >= 101);
        ASSERT_GT(pool->get().base_points() + pool->get().points_per_level() * (kSavedLevel - 1), 101u);
        auto& comp = tlsEcs.actorRegistry.emplace<PlayerPetComp>(player);
        auto* pet = comp.add_pets();
        pet->set_pet_id(kPetId);
        pet->set_pet_table_id(petTableId);
        pet->set_level(kSavedLevel);
        pet->set_health(123);
        pet->set_mana(234);
        (*pet->mutable_allocated())[dimensionId] = 100;
        (*pet->mutable_aptitude())[dimensionId] = 10000;
    }
    PlayerPetComp& Comp() { return tlsEcs.actorRegistry.get<PlayerPetComp>(player); }
    static constexpr uint64_t kPetId = 900001;
    static constexpr uint32_t kSavedLevel = 85;
    std::optional<std::reference_wrapper<const AttributePoolTable>> pool;
    uint32_t dimensionId{0};
};

TEST_F(PetOwnerLevelTest, MissingOwnerLevelSkipsLoadRepairAndRecalculationWithoutChangingSavedPet) {
    // 加一个失效出战引用和失效维度,证明守卫在 InitializeOnLoad 的清理副作用之前。
    Comp().set_active_pet_id(kPetId + 1);
    (*Comp().mutable_pets(0)->mutable_allocated())[std::numeric_limits<uint32_t>::max()] = 7;
    const auto before = Comp().SerializeAsString();
    PetSystem::InitializeOnLoad(player);
    EXPECT_EQ(before, Comp().SerializeAsString());
    PetSystem::RecalculateAll(player, PetSystem::RecalcReason::kLevelChanged);
    EXPECT_EQ(before, Comp().SerializeAsString());
    EXPECT_FALSE(Has<LevelComp>(player));
}

TEST_F(PetOwnerLevelTest, MissingOwnerLevelRejectsSummonBeforeChangingActivePet) {
    const auto before = Comp().SerializeAsString();
    EXPECT_EQ(kServiceUnavailable, PetSystem::Summon(player, kPetId));
    EXPECT_EQ(before, Comp().SerializeAsString());
    EXPECT_FALSE(Has<LevelComp>(player));
}

TEST_F(PetOwnerLevelTest, MissingOwnerLevelRejectsAllocationBeforeChangingPointsOrLevel) {
    const auto before = Comp().SerializeAsString();
    EXPECT_EQ(kServiceUnavailable, PetSystem::Allocate(player, kPetId, {{dimensionId, 101}}));
    EXPECT_EQ(before, Comp().SerializeAsString());
    EXPECT_FALSE(Has<LevelComp>(player));
}

TEST_F(PetOwnerLevelTest, MissingOwnerLevelRejectsResetBeforeChargingOrClearingPoints) {
    ASSERT_GT(pool->get().reset_cost_gold(), 0u);
    ASSERT_LE(pool->get().reset_free_below_level(), kSavedLevel);
    auto& currency = tlsEcs.actorRegistry.emplace<CurrencyComp>(player);
    for (uint32_t index = 0; index < kCurrencyMax; ++index) currency.add_values(index == kCurrencyGold ? 10000 : 0);
    ASSERT_GT(currency.values(kCurrencyGold), pool->get().reset_cost_gold());
    auto& debt = tlsEcs.actorRegistry.emplace<PlayerCurrencyComp>(player);
    const auto petBefore = Comp().SerializeAsString();
    const auto currencyBefore = currency.SerializeAsString();
    EXPECT_EQ(kServiceUnavailable, PetSystem::Reset(player, kPetId));
    EXPECT_EQ(petBefore, Comp().SerializeAsString());
    EXPECT_EQ(currencyBefore, currency.SerializeAsString());
    EXPECT_FALSE(debt.dirty);
    EXPECT_FALSE(Has<LevelComp>(player));
}

TEST_F(PetOwnerLevelTest, ListUsesSavedPetLevelWithoutRequiringOwnerLevel) {
    const auto before = Comp().SerializeAsString();
    PetListInfo list;
    ASSERT_EQ(kSuccess, PetSystem::BuildList(player, list));
    ASSERT_EQ(1, list.pets_size());
    EXPECT_EQ(kSavedLevel, list.pets(0).level());
    EXPECT_EQ(before, Comp().SerializeAsString());
    EXPECT_FALSE(Has<LevelComp>(player));
}
} // namespace
