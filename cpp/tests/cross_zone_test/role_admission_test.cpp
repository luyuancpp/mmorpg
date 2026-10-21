#include <gtest/gtest.h>
#include <iomanip>
#include <memory>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>
#include "core/security/service_call_auth.h"
#include "core/type_alias/player_session_type_alias.h"
#include "engine/core/type_define/player_guid_comp.h"
#include "player/system/player_lifecycle.h"
#include "player/system/role_admission_system.h"
#include "battle/system/player_battle.h"
#include "player/comp/player_frozen_comp.h"
#include "proto/common/component/battle_comp.pb.h"
#include "proto/scene/scene.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "spatial/comp/scene_node_scene_comp.h"
#include "thread_context/ecs_context.h"

namespace {
using Admission = RoleAdmissionSystem;
using namespace std::chrono_literals;
constexpr Guid kPlayer = 970400001;
constexpr uint64_t kScene = 970400002;
constexpr uint64_t kRevision = 17;

std::string Version(uint64_t epoch, uint64_t seq) {
    std::ostringstream text;
    text << std::setfill('0') << std::setw(20) << epoch << ':' << std::setw(20) << seq;
    return text.str();
}

class RoleAdmissionTest : public testing::Test {
protected:
    void SetUp() override {
        scene = tlsEcs.sceneRegistry.create();
        tlsEcs.sceneRegistry.emplace<SceneInfoComp>(scene).set_scene_id(kScene);
        Admission::Dependencies dependencies;
        dependencies.now = [this] { return now; };
        dependencies.localNode = [] { return Admission::LocalNode{"01234567-89ab-cdef-0123-456789abcdef", "91", 3}; };
        dependencies.registerAdmission = [this](auto request, auto reply) {
            registers.push_back(std::move(request)); registerReplies.push_back(std::move(reply)); return true;
        };
        dependencies.closeAdmissions = [this](auto request, auto reply) {
            closes.push_back(std::move(request)); closeReplies.push_back(std::move(reply)); return true;
        };
        dependencies.ackDrain = [this](auto request, auto reply) {
            acks.push_back(std::move(request)); ackReplies.push_back(std::move(reply)); return true;
        };
        dependencies.readSqlAck = [this](Guid player, auto reply) {
            EXPECT_EQ(kPlayer, player); sqlReplies.push_back(std::move(reply)); return true;
        };
        dependencies.activate = [this](Guid player, PlayerEnterContext context) {
            EXPECT_EQ(kPlayer, player); activated.push_back(std::move(context));
        };
        dependencies.freeze = [this](Guid player) { EXPECT_EQ(kPlayer, player); ++freezes; };
        dependencies.reject = [this](Guid player, uint32_t) { EXPECT_TRUE(player == 0 || player == kPlayer); ++rejections; };
        dependencies.hasPendingLoad = [this](Guid) { return pendingLoad; };
        scope = std::make_unique<Admission::ScopedDependencies>(std::move(dependencies));
    }
    void TearDown() override {
        RemoveEntity();
        PlayerLifecycleSystem::GetPendingEnterMap().erase(kPlayer);
        PlayerLifecycleSystem::EraseSessionMappingIfOwned(91, kPlayer);
        if (tlsEcs.sceneRegistry.valid(scene)) tlsEcs.sceneRegistry.destroy(scene);
        scope.reset();
    }
    PlayerEnterContext Context() const {
        PlayerEnterContext context;
        context.enterInfo.set_scene_id(kScene);
        context.enterInfo.set_session_id(91);
        context.ownerRevision = kRevision; context.ownerEpoch = 8; context.loginGeneration = 9;
        return context;
    }
    void Enter() { Admission::RequestEnter(kPlayer, Context()); }
    void CompleteRegister(size_t index, bool ok = true, uint64_t closed = 0) {
        ASSERT_LT(index, registerReplies.size());
        data_service::RegisterRoleAdmissionResponse reply;
        *reply.mutable_identity() = registers[index].identity();
        reply.set_closed_through(closed);
        auto completed = std::move(registerReplies[index]);
        ASSERT_TRUE(static_cast<bool>(completed));
        completed(ok, ok ? &reply : nullptr);
    }
    common::base::RoleTradeSeal SealFor(size_t index = 0, uint64_t listing = 71) const {
        const auto& identity = registers.at(index).identity();
        common::base::RoleTradeSeal seal;
        seal.set_listing_id(listing); seal.set_player_id(identity.player_id());
        seal.set_owner_revision(identity.owner_revision()); seal.set_scene_instance_id(identity.scene_instance_id());
        seal.set_through_generation(identity.generation()); seal.set_scene_node_id(identity.scene_node_id());
        seal.set_zone_id(identity.zone_id());
        return seal;
    }
    void Tick() { now += 2s; Admission::Tick(); }
    void Sql(size_t index, uint64_t epoch, uint64_t seq) {
        ASSERT_LT(index, sqlReplies.size());
        auto completed = std::move(sqlReplies[index]);
        ASSERT_TRUE(static_cast<bool>(completed));
        const auto version = Version(epoch, seq);
        completed(Admission::SqlAck{std::to_string(epoch), version, version, version});
    }
    void AddEntity() {
        entity = tlsEcs.actorRegistry.create();
        tlsEcs.actorRegistry.emplace<PlayerGuidComp>(entity, kPlayer);
        tlsEcs.playerList[kPlayer] = entity;
    }
    void RemoveEntity() {
        if (tlsEcs.actorRegistry.valid(entity)) tlsEcs.actorRegistry.destroy(entity);
        const auto found = tlsEcs.playerList.find(kPlayer);
        if (found != tlsEcs.playerList.end() && found->second == entity) tlsEcs.playerList.erase(found);
        entity = entt::null;
    }
    SceneEntity scene{entt::null};
    entt::entity entity{entt::null};
    Admission::Clock::time_point now{Admission::Clock::time_point{} + 1h};
    std::unique_ptr<Admission::ScopedDependencies> scope;
    std::vector<data_service::RegisterRoleAdmissionRequest> registers;
    std::vector<Admission::RegisterReply> registerReplies;
    std::vector<data_service::CloseRoleAdmissionsRequest> closes;
    std::vector<Admission::CloseReply> closeReplies;
    std::vector<data_service::AckRoleTradeDrainRequest> acks;
    std::vector<std::function<void(bool)>> ackReplies;
    std::vector<std::function<void(std::optional<Admission::SqlAck>)>> sqlReplies;
    std::vector<PlayerEnterContext> activated;
    uint32_t freezes{0}, rejections{0};
    bool pendingLoad{false};
};

TEST_F(RoleAdmissionTest, LiveBattleFinishesBeforeTradeFreezesOrUnloadsThePlayer) {
    AddEntity();
    tlsEcs.actorRegistry.emplace<InBattleComp>(entity);
    PlayerLifecycleSystem::FreezeRoleForTrade(kPlayer);
    EXPECT_TRUE(tlsEcs.actorRegistry.valid(entity));
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<PlayerFrozenComp>(entity));
    EXPECT_TRUE(sqlReplies.empty());
    EXPECT_TRUE(acks.empty());
}

TEST_F(RoleAdmissionTest, AcceptedTradeSealRejectsNewBattleBeforeDrainCompletes) {
    Enter();
    CompleteRegister(0);
    AddEntity();
    ASSERT_TRUE(Admission::Seal(SealFor()));
    ASSERT_FALSE(tlsEcs.actorRegistry.any_of<PlayerFrozenComp>(entity));
    PrepareBattleRequest request;
    request.set_player_id(kPlayer);
    request.set_battle_id(83);
    request.set_deadline_ms(std::numeric_limits<uint64_t>::max());
    PrepareBattleResponse response;
    PlayerBattleSystem::PrepareBattle(request, response);
    EXPECT_EQ(kFeatureUnavailable, response.error_message().id());
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<InBattleComp>(entity));
    EXPECT_TRUE(acks.empty());
}

TEST_F(RoleAdmissionTest, MissingIdentityAndSceneFailBeforeRegistration) {
    for (unsigned field = 0; field != 6; ++field) {
        auto context = Context();
        switch (field) {
        case 0: context.ownerRevision = 0; break;
        case 1: context.ownerEpoch = 0; break;
        case 2: context.loginGeneration = 0; break;
        case 3: context.enterInfo.set_session_id(0); break;
        case 4: context.enterInfo.set_scene_id(0); break;
        default: break;
        }
        Admission::RequestEnter(field == 5 ? Guid{0} : kPlayer, context);
    }
    EXPECT_EQ(6u, rejections);
    EXPECT_TRUE(registers.empty()); EXPECT_TRUE(activated.empty());
    EXPECT_FALSE(Admission::Allows(kPlayer, 1, kRevision));
}

TEST_F(RoleAdmissionTest, UnknownRegisterReplaysOriginalAndNeverActivatesBeforeConfirmation) {
    Enter();
    ASSERT_EQ(1u, registers.size());
    const auto generation = registers.front().identity().generation();
    EXPECT_NE(0u, generation); EXPECT_TRUE(activated.empty());
    EXPECT_FALSE(Admission::Allows(kPlayer, generation, kRevision));
    CompleteRegister(0, false); Tick();
    ASSERT_EQ(2u, registers.size());
    EXPECT_EQ(registers[0].SerializeAsString(), registers[1].SerializeAsString());
    CompleteRegister(1);
    ASSERT_EQ(1u, activated.size());
    EXPECT_TRUE(Admission::Allows(kPlayer, generation, kRevision));
    EXPECT_FALSE(Admission::Allows(kPlayer, 0, kRevision));
    EXPECT_FALSE(Admission::Allows(kPlayer, generation, 0));
    EXPECT_FALSE(Admission::Allows(kPlayer, generation, kRevision + 1));
    EXPECT_EQ(generation, activated.front().roleAdmissionGeneration);
}

TEST_F(RoleAdmissionTest, UnknownRegisterDoesNotChangeGenerationWhenAnotherRouteArrives) {
    Enter(); Enter();
    EXPECT_EQ(1u, registers.size()); EXPECT_EQ(0u, rejections);
    auto changed = Context(); changed.ownerRevision += 1;
    Admission::RequestEnter(kPlayer, changed);
    EXPECT_EQ(1u, registers.size()); EXPECT_EQ(1u, rejections);
    CompleteRegister(0);
    EXPECT_EQ(1u, activated.size());
}

TEST_F(RoleAdmissionTest, ThreeFailedAttemptsStopWithoutActivation) {
    Enter(); CompleteRegister(0, false); Tick();
    CompleteRegister(1, false); Tick(); CompleteRegister(2, false);
    Tick();
    EXPECT_EQ(3u, registers.size()); EXPECT_EQ(1u, rejections); EXPECT_TRUE(activated.empty());
}

TEST_F(RoleAdmissionTest, LateSuccessAfterDeadlineCannotActivateEvenBeforeTick) {
    Enter(); now += 16s;
    CompleteRegister(0);
    EXPECT_TRUE(activated.empty()); EXPECT_EQ(1u, rejections);
}

TEST_F(RoleAdmissionTest, ClosedThroughInRegisterReplyNeverGrantsThatGeneration) {
    Enter();
    CompleteRegister(0, true, registers[0].identity().generation());
    EXPECT_TRUE(activated.empty());
    EXPECT_FALSE(Admission::Allows(kPlayer, registers[0].identity().generation(), kRevision));
}

TEST_F(RoleAdmissionTest, CallbackFromDestroyedScopeCannotMutateRestoredThreadState) {
    Enter();
    auto callback = std::move(registerReplies.front());
    data_service::RegisterRoleAdmissionResponse reply;
    *reply.mutable_identity() = registers.front().identity();
    scope.reset();
    callback(true, &reply);
    EXPECT_TRUE(activated.empty());
    EXPECT_FALSE(Admission::Allows(kPlayer, reply.identity().generation(), kRevision));
}

TEST_F(RoleAdmissionTest, DrainWaitsForEntityCallbacksAndActualForcedSave) {
    Enter(); CompleteRegister(0); AddEntity();
    Admission::ObserveSave(kPlayer, 8, 2);
    Admission::BeginCallback(kPlayer);
    ASSERT_TRUE(Admission::Seal(SealFor()));
    EXPECT_TRUE(Admission::IsSealed(kPlayer));
    EXPECT_FALSE(Admission::Allows(kPlayer, registers[0].identity().generation(), kRevision));
    Tick(); EXPECT_TRUE(sqlReplies.empty()); EXPECT_TRUE(acks.empty());
    RemoveEntity(); Admission::OnDestroyed(kPlayer); Tick();
    EXPECT_TRUE(sqlReplies.empty());
    Admission::EndCallback(kPlayer); Tick();
    EXPECT_TRUE(sqlReplies.empty()) << "未成功排入强制存盘，旧SQL满足也不能排空";
    now += 90s; Tick();
    EXPECT_TRUE(Admission::IsSealed(kPlayer)); EXPECT_TRUE(acks.empty());
    Admission::ObserveTradeSaveQueued(kPlayer, 8, 3); Tick();
    ASSERT_EQ(1u, sqlReplies.size()); Sql(0, 8, 3); Tick();
    ASSERT_EQ(1u, acks.size());
    EXPECT_EQ(3u, acks[0].required_save_seq());
}

TEST_F(RoleAdmissionTest, StrictAckNeedsExactBothRowsAndCompleteUint64Version) {
    Enter(); CompleteRegister(0);
    constexpr uint64_t seq = UINT64_MAX;
    Admission::ObserveSave(kPlayer, 8, seq);
    ASSERT_TRUE(Admission::Seal(SealFor()));
    ASSERT_EQ(1u, sqlReplies.size());
    const auto version = Version(8, seq);
    auto callback = std::move(sqlReplies[0]);
    callback(Admission::SqlAck{"8", version, version, Version(8, seq - 1)});
    Tick(); EXPECT_TRUE(acks.empty());
    ASSERT_EQ(2u, sqlReplies.size());
    auto malformed = std::move(sqlReplies[1]);
    malformed(Admission::SqlAck{"08", version, version, version});
    Tick(); EXPECT_TRUE(acks.empty());
    ASSERT_EQ(3u, sqlReplies.size()); Sql(2, 8, seq); Tick();
    ASSERT_EQ(1u, acks.size()); EXPECT_EQ(seq, acks.front().required_save_seq());
}

TEST_F(RoleAdmissionTest, LateLoadCannotActivateAndItsOriginalFenceRaisesDrainFloor) {
    Enter(); CompleteRegister(0);
    auto context = activated.back();
    context.inheritClear.outstandingReplies = 1;
    PlayerLifecycleSystem::GetPendingEnterMap()[kPlayer] = context;
    SessionMap()[91] = kPlayer;
    pendingLoad = true;
    Admission::BeginCallback(kPlayer);
    ASSERT_TRUE(Admission::Seal(SealFor()));
    PlayerAllData loaded;
    loaded.mutable_player_database_data()->set_player_id(kPlayer);
    loaded.mutable_player_database_data()->set_fence_owner_epoch(9);
    loaded.mutable_player_database_data()->set_fence_save_seq(2);
    loaded.mutable_player_database_1_data()->set_player_id(kPlayer);
    loaded.mutable_player_database_1_data()->set_fence_owner_epoch(9);
    loaded.mutable_player_database_1_data()->set_fence_save_seq(2);
    PlayerLifecycleSystem::HandlePlayerAsyncLoaded(kPlayer, loaded);
    EXPECT_EQ(1u, activated.size()); EXPECT_TRUE(sqlReplies.empty());
    Tick(); EXPECT_TRUE(acks.empty());
    pendingLoad = false; Admission::EndCallback(kPlayer);
    auto& pending = PlayerLifecycleSystem::GetPendingEnterMap();
    pending.at(kPlayer).inheritClear.outstandingReplies = 0;
    Tick();
    EXPECT_EQ(0u, pending.count(kPlayer)); EXPECT_FALSE(SessionMap().contains(91));
    ASSERT_EQ(1u, sqlReplies.size()); Sql(0, 9, 1); Tick();
    EXPECT_TRUE(acks.empty());
    ASSERT_EQ(2u, sqlReplies.size()); Sql(1, 9, 2); Tick();
    ASSERT_EQ(1u, acks.size()); EXPECT_EQ(9u, acks[0].required_owner_epoch()); EXPECT_EQ(2u, acks[0].required_save_seq());
}

TEST_F(RoleAdmissionTest, OldHolderSealDuringUnconfirmedNewRegisterStillFreezesOriginal) {
    Enter(); CompleteRegister(0); AddEntity();
    Enter(); ASSERT_EQ(2u, registers.size());
    const auto oldSeal = SealFor();
    ASSERT_TRUE(Admission::Seal(oldSeal));
    EXPECT_TRUE(Admission::IsSealed(kPlayer));
    EXPECT_EQ(1u, freezes);
    CompleteRegister(1, false);
    EXPECT_EQ(1u, activated.size());
    Admission::ObserveTradeSaveQueued(kPlayer, 8, 2); RemoveEntity(); Tick();
    ASSERT_EQ(1u, sqlReplies.size()); Sql(0, 8, 2); Tick();
    ASSERT_EQ(1u, acks.size());
    EXPECT_EQ(oldSeal.SerializeAsString(), acks[0].seal().SerializeAsString());
    auto ack = std::move(ackReplies[0]); ack(true);

    Enter(); ASSERT_EQ(3u, registers.size());
    CompleteRegister(2, true, oldSeal.through_generation());
    const auto current = registers[2].identity().generation();
    ASSERT_TRUE(Admission::Allows(kPlayer, current, kRevision));
    EXPECT_FALSE(Admission::IsSealed(kPlayer));
    EXPECT_TRUE(Admission::Seal(oldSeal)); EXPECT_EQ(1u, freezes);
    auto changed = oldSeal; changed.set_owner_revision(kRevision + 1);
    EXPECT_FALSE(Admission::Seal(changed));
    EXPECT_TRUE(Admission::Allows(kPlayer, current, kRevision));
}

TEST_F(RoleAdmissionTest, CloseKeepsHistoricalFloorAndClosedAuthorizationNeverReactivates) {
    Enter(); CompleteRegister(0);
    Admission::ObserveSave(kPlayer, 8, 12); Tick();
    ASSERT_EQ(1u, closes.size());
    data_service::CloseRoleAdmissionsResponse reply;
    reply.set_closed_through(closes[0].through_generation());
    auto close = std::move(closeReplies[0]); close(true, &reply);
    EXPECT_FALSE(Admission::Allows(kPlayer, closes[0].through_generation(), kRevision));
    Enter(); CompleteRegister(1, true, reply.closed_through());
    Admission::ObserveSave(kPlayer, 0, 0); Tick();
    ASSERT_EQ(2u, closes.size());
    EXPECT_EQ(8u, closes[1].required_owner_epoch()); EXPECT_EQ(12u, closes[1].required_save_seq());
}

TEST_F(RoleAdmissionTest, HolderCapturedBeforeCloseStillReceivesListingAckAfterClose) {
    Enter(); CompleteRegister(0); Tick();
    ASSERT_EQ(1u, closes.size());
    data_service::CloseRoleAdmissionsResponse reply;
    reply.set_closed_through(closes[0].through_generation());
    auto close = std::move(closeReplies[0]); close(true, &reply);
    ASSERT_TRUE(Admission::Seal(SealFor())); Tick();
    ASSERT_EQ(1u, acks.size()) << "Close不能替代已固定listing holder的ACK";
}

TEST_F(RoleAdmissionTest, FailedAckRetriesExactOriginalAndCannotOpenAdmission) {
    Enter(); CompleteRegister(0);
    ASSERT_TRUE(Admission::Seal(SealFor()));
    ASSERT_EQ(1u, acks.size());
    auto failed = std::move(ackReplies[0]); failed(false);
    Enter(); EXPECT_EQ(1u, registers.size()); EXPECT_EQ(1u, rejections);
    Tick(); ASSERT_EQ(2u, acks.size());
    EXPECT_EQ(acks[0].SerializeAsString(), acks[1].SerializeAsString());
    auto completed = std::move(ackReplies[1]); completed(true);
    Tick(); EXPECT_EQ(2u, acks.size());
    EXPECT_FALSE(Admission::Allows(kPlayer, registers[0].identity().generation(), kRevision));
}

TEST(RoleAdmissionScope, ExplicitNonzeroOriginalIsRequiredAndNestedScopeRestoresState) {
    Admission::Dependencies dependencies;
    auto& confirmed = dependencies.confirmedAdmissions.emplace_back();
    confirmed.set_player_id(kPlayer); confirmed.set_generation(31); confirmed.set_owner_revision(kRevision);
    auto& invalid = dependencies.confirmedAdmissions.emplace_back();
    invalid.set_player_id(kPlayer + 1); invalid.set_owner_revision(kRevision);
    {
        Admission::ScopedDependencies outer(std::move(dependencies));
        EXPECT_TRUE(Admission::Allows(kPlayer, 31, kRevision));
        EXPECT_FALSE(Admission::Allows(kPlayer + 1, 0, kRevision));
        {
            Admission::ScopedDependencies inner(Admission::Dependencies{});
            EXPECT_FALSE(Admission::Allows(kPlayer, 31, kRevision));
        }
        EXPECT_TRUE(Admission::Allows(kPlayer, 31, kRevision));
    }
    EXPECT_FALSE(Admission::Allows(kPlayer, 31, kRevision));
}

TEST(RoleAdmissionAuthentication, SignatureBindsMethodCallerBodyAndDeadline) {
    constexpr std::string_view method = "/scene_node.SceneNodeGrpc/SealAccountRoleTrade";
    constexpr std::string_view secret = "role-admission-test-secret-at-least-32-bytes";
    constexpr uint64_t now = 1900000000123;
    SealAccountRoleTradeRequest request;
    request.mutable_seal()->set_player_id(kPlayer);
    request.mutable_seal()->set_listing_id(71);
    ASSERT_TRUE(service_call_auth::Sign(method, request, *request.mutable_auth(), "trade", secret, now));
    EXPECT_TRUE(service_call_auth::Verify(method, request, request.auth(), "trade", secret, now));
    EXPECT_TRUE(service_call_auth::Verify(method, request, request.auth(), "trade", std::string(" \t") + std::string(secret) + "\r\n", now));
    EXPECT_FALSE(service_call_auth::Verify("/scene_node.SceneNodeGrpc/ReleasePlayer", request, request.auth(), "trade", secret, now));
    EXPECT_FALSE(service_call_auth::Verify(method, request, request.auth(), "scene", secret, now));
    EXPECT_FALSE(service_call_auth::Verify(method, request, request.auth(), "trade", secret, now + 300001));
    EXPECT_FALSE(service_call_auth::Verify(method, request, request.auth(), "trade", "short", now));
    request.mutable_seal()->set_owner_revision(kRevision);
    EXPECT_FALSE(service_call_auth::Verify(method, request, request.auth(), "trade", secret, now));
}
} // namespace
