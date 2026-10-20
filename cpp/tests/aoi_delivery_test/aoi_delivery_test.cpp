// 直接编译生产 AOI/Grid/Interest/View，只替换网络发送边界，观察实际接收者和消息体。
#include <gtest/gtest.h>
#include "actor/actor_net_id.h"
#include "core/system/id_generator.h"
#include "engine/core/type_define/player_guid_comp.h"
#include "engine/core/utils/registry/actor_log.h"
#include "generated/attribute/actorbaseattributess2c_attribute_sync.h"
#include "rpc/service_metadata/player_state_attribute_sync_service_metadata.h"
#include <limits>
#include <vector>
#include "hexagons_grid.h"
#include "spatial/system/aoi.h"
#include "spatial/system/interest.h"
#include "spatial/comp/scene_node_scene_comp.h"
#include "combat/buff/comp/buff_comp.h"
#include "modules/scene/comp/scene_comp.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/base_comp.pb.h"
#include "proto/common/component/player_comp.pb.h"
#include "proto/scene/player_scene.pb.h"
#include "rpc/service_metadata/player_scene_service_metadata.h"
#include "thread_context/ecs_context.h"

namespace {
struct Delivery {
    uint32_t messageId;
    entt::entity receiver;
    std::string body;
};
std::vector<Delivery> deliveries;

int CreatesFor(entt::entity receiver, entt::entity target) {
    int count = 0;
    for (const auto& sent : deliveries) {
        if (sent.receiver != receiver || sent.messageId != SceneSceneClientPlayerNotifyActorListCreateMessageId) continue;
        ActorListCreateS2C message;
        EXPECT_TRUE(message.ParseFromString(sent.body));
        for (const auto& actor : message.actor_list()) if (actor.entity() == actor_net_id::Of(tlsEcs.actorRegistry, target)) ++count;
    }
    return count;
}

int DestroysFor(entt::entity receiver, entt::entity target) {
    int count = 0;
    for (const auto& sent : deliveries) {
        if (sent.receiver != receiver || sent.messageId != SceneSceneClientPlayerNotifyActorListDestroyMessageId) continue;
        ActorListDestroyS2C message;
        EXPECT_TRUE(message.ParseFromString(sent.body));
        for (auto actor : message.entity()) if (actor == actor_net_id::Of(tlsEcs.actorRegistry, target)) ++count;
    }
    return count;
}

class AoiDeliveryTest : public ::testing::Test {
protected:
    SceneEntity scene = entt::null;
    void SetUp() override {
        tlsEcs.Clear();
        tlsIdGeneratorManager.SetNodeId(93);
        actor_net_id::Install(tlsEcs.actorRegistry);
        deliveries.clear();
        scene = tlsEcs.sceneRegistry.create();
    }
    void TearDown() override { tlsEcs.Clear(); deliveries.clear(); }
    entt::entity Spawn(double x = 0, double y = 0) {
        auto entity = tlsEcs.actorRegistry.create();
        EXPECT_NE(0, actor_net_id::Assign(tlsEcs.actorRegistry, entity));
        auto& transform = tlsEcs.actorRegistry.emplace<Transform>(entity);
        transform.mutable_location()->set_x(x);
        transform.mutable_location()->set_y(y);
        tlsEcs.actorRegistry.emplace<SceneEntityComp>(entity, SceneEntityComp{scene});
        tlsEcs.actorRegistry.emplace<Player>(entity);
        tlsEcs.actorRegistry.emplace<PlayerGuidComp>(entity, 10000 + entt::to_integral(entity));
        return entity;
    }
};
}

// 测试只截获既有发送函数；生产 AOI 和可见性/容量/隐身判定未替换。
void SendMessageToClientViaGate(uint32_t id, const google::protobuf::Message& message, entt::entity receiver) {
    deliveries.push_back({id, receiver, message.SerializeAsString()});
}
void BroadcastToPlayerClients(uint32_t id, const google::protobuf::Message& message, const EntityUnorderedSet& receivers) {
    for (auto receiver : receivers) SendMessageToClientViaGate(id, message, receiver);
}
void BroadcastToPlayerClients(uint32_t id, const google::protobuf::Message& message, const EntityVector& receivers) {
    for (auto receiver : receivers) SendMessageToClientViaGate(id, message, receiver);
}

TEST_F(AoiDeliveryTest, EmptySceneAndLoneActorCreateOnlyOccupiedGridState) {
    AoiSystem::Update(0);
    EXPECT_FALSE(tlsEcs.sceneRegistry.any_of<SceneGridListComp>(scene));
    const auto loneActor = Spawn();
    AoiSystem::Update(0);
    EXPECT_TRUE(tlsEcs.sceneRegistry.any_of<SceneGridListComp>(scene));
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<AoiListComp>(loneActor));
    EXPECT_TRUE(deliveries.empty());
}

TEST_F(AoiDeliveryTest, DestroyedTargetDoesNotCreateWatchersInterestList) {
    const auto watcher = Spawn();
    const auto target = Spawn();
    tlsEcs.actorRegistry.destroy(target);
    EXPECT_FALSE(InterestSystem::AddAoiEntity(watcher, target));
    EXPECT_FALSE(InterestSystem::AddAoiEntity(watcher, target, AoiPriority::kPinned));
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<AoiListComp>(watcher));
    EXPECT_TRUE(deliveries.empty());
}

TEST_F(AoiDeliveryTest, StationaryObserverReceivesNewEntrantWithoutMoving) {
    auto stationary = Spawn();
    AoiSystem::Update(0);
    deliveries.clear();
    auto entrant = Spawn();
    AoiSystem::Update(0);
    EXPECT_EQ(CreatesFor(stationary, entrant), 1) << "既有静止玩家必须收到后来者的 ActorCreate";
    EXPECT_EQ(CreatesFor(entrant, stationary), 1);
    EXPECT_EQ(CreatesFor(stationary, stationary), 0);
    deliveries.clear();
    for (int frame = 0; frame < 10; ++frame) AoiSystem::Update(0.1);
    EXPECT_TRUE(deliveries.empty()) << "稳定视野不得每帧重复发送出生消息";
}

TEST_F(AoiDeliveryTest, MovingOutNotifiesBothObservers) {
    auto stationary = Spawn();
    AoiSystem::Update(0);
    auto mover = Spawn();
    AoiSystem::Update(0);
    deliveries.clear();
    auto* transform = tlsEcs.actorRegistry.try_get<Transform>(mover);
    ASSERT_NE(transform, nullptr);
    transform->mutable_location()->set_x(1000);
    transform->mutable_location()->set_y(1000);
    AoiSystem::Update(0);
    EXPECT_EQ(DestroysFor(stationary, mover), 1) << "静止观察者也必须清除移出视野的角色";
    EXPECT_EQ(DestroysFor(mover, stationary), 1);
}

TEST_F(AoiDeliveryTest, HiddenEntrantIsNotBroadcastToObserver) {
    auto stationary = Spawn();
    AoiSystem::Update(0);
    auto entrant = Spawn();
    tlsEcs.actorRegistry.emplace<StealthedTagComp>(entrant);
    deliveries.clear();
    AoiSystem::Update(0);
    EXPECT_EQ(CreatesFor(stationary, entrant), 0);
    EXPECT_EQ(CreatesFor(entrant, stationary), 1);
}

TEST_F(AoiDeliveryTest, FullObserverDoesNotReceiveRejectedNormalEntrant) {
    auto stationary = Spawn();
    AoiSystem::Update(0);
    tlsEcs.actorRegistry.emplace<AoiClientCapacityComp>(stationary, AoiClientCapacityComp{kAoiListCapacityMin});
    for (uint32_t i = 0; i < kAoiListCapacityMin; ++i) {
        auto pinned = tlsEcs.actorRegistry.create();
        ASSERT_TRUE(InterestSystem::AddAoiEntity(stationary, pinned, AoiPriority::kPinned));
    }
    auto entrant = Spawn();
    deliveries.clear();
    AoiSystem::Update(0);
    EXPECT_EQ(CreatesFor(stationary, entrant), 0);
    EXPECT_EQ(CreatesFor(entrant, stationary), 1);
}

TEST_F(AoiDeliveryTest, ReversePinnedEntryIsNotDestroyedOnSpatialLeave) {
    auto stationary = Spawn();
    AoiSystem::Update(0);
    auto mover = Spawn();
    AoiSystem::Update(0);
    ASSERT_TRUE(InterestSystem::AddAoiEntity(stationary, mover, AoiPriority::kPinned));
    deliveries.clear();
    auto* transform = tlsEcs.actorRegistry.try_get<Transform>(mover);
    ASSERT_NE(transform, nullptr);
    transform->mutable_location()->set_x(1000);
    transform->mutable_location()->set_y(1000);
    AoiSystem::Update(0);
    EXPECT_EQ(DestroysFor(stationary, mover), 0);
    EXPECT_EQ(DestroysFor(mover, stationary), 1);
}

TEST_F(AoiDeliveryTest, VisibilityAndLeaveNotificationsCarryCorrectBodies) {
    auto stationary = Spawn();
    AoiSystem::Update(0);
    auto mover = Spawn();
    AoiSystem::Update(0); // ActorListCreate
    auto* transform = tlsEcs.actorRegistry.try_get<Transform>(mover);
    ASSERT_NE(transform, nullptr);
    transform->mutable_location()->set_x(1000);
    transform->mutable_location()->set_y(1000);
    AoiSystem::Update(0); // ActorListDestroy
    auto returner = Spawn();
    AoiSystem::Update(0);
    AoiSystem::OnLeaveScene(returner); // ActorDestroy(BroadcastEntityLeave)

    // 前置条件:三条路径确实都发过消息,且消息体正确。
    ASSERT_EQ(CreatesFor(stationary, mover), 1);
    ASSERT_EQ(DestroysFor(stationary, mover), 1);
    int actorDestroyToStationary = 0;
    for (const auto& sent : deliveries) {
        if (sent.receiver != stationary || sent.messageId != SceneSceneClientPlayerNotifyActorDestroyMessageId) continue;
        ActorDestroyS2C message;
        ASSERT_TRUE(message.ParseFromString(sent.body));
        EXPECT_EQ(message.entity(), actor_net_id::Of(tlsEcs.actorRegistry, returner));
        ++actorDestroyToStationary;
    }
    ASSERT_EQ(actorDestroyToStationary, 1);
}

TEST_F(AoiDeliveryTest, LeaveTouchesOnlyNeighborGridObservers) {
    const auto target = Spawn(0, 0);
    const auto nearby = Spawn(1, 0);
    const auto otherScene = tlsEcs.sceneRegistry.create();
    const auto elsewhere = Spawn(0, 0);
    tlsEcs.actorRegistry.get<SceneEntityComp>(elsewhere).sceneEntity = otherScene;
    AoiSystem::Update(0);
    // 普通非邻格条目代表另一个场景的观察者,不能让目标离场退化为扫描全节点。
    ASSERT_TRUE(InterestSystem::AddAoiEntity(elsewhere, target));
    deliveries.clear();

    AoiSystem::OnLeaveScene(target);

    EXPECT_EQ(0, DestroysFor(elsewhere, target));
    EXPECT_TRUE(tlsEcs.actorRegistry.get<AoiListComp>(elsewhere).Contains(target))
        << "离场只访问邻格观察者,不扫全节点";
    EXPECT_FALSE(tlsEcs.actorRegistry.get<AoiListComp>(nearby).Contains(target));
}





TEST_F(AoiDeliveryTest, BaseAttributeSyncCarriesPlayerGuidAndClearsMask) {
    const auto actor = Spawn(17, 23);
    const auto receiver = Spawn(1000, 1000);
    const auto* playerGuid = tlsEcs.actorRegistry.try_get<PlayerGuidComp>(actor);
    ASSERT_NE(nullptr, playerGuid);
    const Guid expectedPlayerId = playerGuid->value;
    SetActorBaseAttributesS2CAttrDirtyBit(actor, ActorBaseAttributesS2C::kEntityIdFieldNumber);
    SetActorBaseAttributesS2CAttrDirtyBit(actor, ActorBaseAttributesS2C::kTransformFieldNumber);
    ActorBaseAttributesS2CSyncAttributes(actor, ScenePlayerSyncSyncBaseAttributeMessageId, EntityUnorderedSet{receiver});
    ASSERT_EQ(1u, deliveries.size());
    EXPECT_EQ(receiver, deliveries[0].receiver);
    EXPECT_EQ(ScenePlayerSyncSyncBaseAttributeMessageId, deliveries[0].messageId);
    ActorBaseAttributesS2C message;
    ASSERT_TRUE(message.ParseFromString(deliveries[0].body));
    EXPECT_EQ(expectedPlayerId, message.entity_id());
    EXPECT_EQ(17, message.transform().location().x());
    EXPECT_EQ(23, message.transform().location().y());
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<ActorBaseAttributesS2CDirtyMaskComp>(actor));
}

TEST_F(AoiDeliveryTest, BaseAttributeSyncCarriesPlayerGuidWhenOnlyTransformIsDirty) {
    const auto actor = Spawn(17, 23);
    const auto receiver = Spawn(1000, 1000);
    const auto* playerGuid = tlsEcs.actorRegistry.try_get<PlayerGuidComp>(actor);
    ASSERT_NE(nullptr, playerGuid);
    const Guid expectedPlayerId = playerGuid->value;
    SetActorBaseAttributesS2CAttrDirtyBit(actor, ActorBaseAttributesS2C::kTransformFieldNumber);

    ActorBaseAttributesS2CSyncAttributes(actor, ScenePlayerSyncSyncBaseAttributeMessageId, EntityUnorderedSet{receiver});

    ASSERT_EQ(1u, deliveries.size());
    EXPECT_EQ(receiver, deliveries[0].receiver);
    EXPECT_EQ(ScenePlayerSyncSyncBaseAttributeMessageId, deliveries[0].messageId);
    ActorBaseAttributesS2C message;
    ASSERT_TRUE(message.ParseFromString(deliveries[0].body));
    EXPECT_EQ(expectedPlayerId, message.entity_id());
    EXPECT_TRUE(message.has_transform());
    EXPECT_EQ(17, message.transform().location().x());
    EXPECT_EQ(23, message.transform().location().y());
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<ActorBaseAttributesS2CDirtyMaskComp>(actor));
}

TEST_F(AoiDeliveryTest, AttributeSyncKeepsDirtyStateWithoutRecipientsThenConsumesMissingFields) {
    const auto actor = tlsEcs.actorRegistry.create();
    const auto receiver = Spawn();
    SetActorBaseAttributesS2CAttrDirtyBit(actor, ActorBaseAttributesS2C::kEntityIdFieldNumber);
    ActorBaseAttributesS2CSyncAttributes(actor, ScenePlayerSyncSyncBaseAttributeMessageId, EntityVector{});
    EXPECT_TRUE(tlsEcs.actorRegistry.any_of<ActorBaseAttributesS2CDirtyMaskComp>(actor));
    ActorBaseAttributesS2CSyncAttributes(actor, ScenePlayerSyncSyncBaseAttributeMessageId, EntityVector{receiver});
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<ActorBaseAttributesS2CDirtyMaskComp>(actor));
    EXPECT_TRUE(deliveries.empty()) << "缺少本轮标脏字段时不发空包,仍须消费脏标记";
}

TEST_F(AoiDeliveryTest, AttributeSyncConsumesAnEmptyDirtyMarker) {
    const auto actor = Spawn();
    const auto receiver = Spawn();
    tlsEcs.actorRegistry.emplace<ActorBaseAttributesS2CDirtyMaskComp>(actor);
    ActorBaseAttributesS2CSyncAttributes(actor, ScenePlayerSyncSyncBaseAttributeMessageId, EntityVector{receiver});
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<ActorBaseAttributesS2CDirtyMaskComp>(actor));
    EXPECT_TRUE(deliveries.empty());
}

TEST_F(AoiDeliveryTest, DirtyBitOnDestroyedEntityCreatesNothing) {
    const auto actor = tlsEcs.actorRegistry.create();
    tlsEcs.actorRegistry.destroy(actor);
    SetActorBaseAttributesS2CAttrDirtyBit(actor, ActorBaseAttributesS2C::kEntityIdFieldNumber);
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<ActorBaseAttributesS2CDirtyMaskComp>(actor));
    const auto liveActor = tlsEcs.actorRegistry.create();
    SetActorBaseAttributesS2CAttrDirtyBit(liveActor, std::numeric_limits<std::size_t>::max());
    EXPECT_FALSE(tlsEcs.actorRegistry.any_of<ActorBaseAttributesS2CDirtyMaskComp>(liveActor));
}

TEST_F(AoiDeliveryTest, ActorLogKeepsPlayerIdentityAndFullEntitySeparate) {
    const auto actor = Spawn();
    auto* guid = tlsEcs.actorRegistry.try_get<PlayerGuidComp>(actor);
    ASSERT_NE(nullptr, guid);
    guid->value = (uint64_t{1} << 63) + 123;
    const Guid playerId = guid->value;
    muduo::LogStream live;
    live << ActorLog(actor);
    EXPECT_EQ("player_id=" + std::to_string(playerId) + " entity=" + std::to_string(entt::to_integral(actor)),
              live.buffer().toString());
    tlsEcs.actorRegistry.destroy(actor);
    muduo::LogStream destroyed;
    destroyed << ActorLog(actor);
    EXPECT_EQ("player_id=0 entity=" + std::to_string(entt::to_integral(actor)), destroyed.buffer().toString());
}
