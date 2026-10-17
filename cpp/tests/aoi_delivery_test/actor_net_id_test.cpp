#include <gtest/gtest.h>

#include "actor/actor_net_id.h"
#include "core/system/id_generator.h"

namespace {
class ActorNetIdTest : public ::testing::Test
{
protected:
    void SetUp() override { tlsIdGeneratorManager.SetNodeId(101); }
};
}

TEST_F(ActorNetIdTest, MissingInstallationOrNodeIdRefusesWithoutAttachingIdentity)
{
    entt::registry actors;
    const auto actor = actors.create();
    EXPECT_EQ(0, actor_net_id::Assign(actors, actor));
    EXPECT_EQ(0, actor_net_id::Of(actors, actor));
    EXPECT_TRUE(actor_net_id::Resolve(actors, 1) == entt::null);
    EXPECT_EQ(0, actor_net_id::Of(actors, actor));

    actor_net_id::Install(actors);
    tlsIdGeneratorManager.SetNodeId(0);
    EXPECT_EQ(0, actor_net_id::Assign(actors, actor));
    EXPECT_EQ(0, actor_net_id::Of(actors, actor));
    tlsIdGeneratorManager.SetNodeId(101);
    EXPECT_NE(0, actor_net_id::Assign(actors, actor));
}

TEST_F(ActorNetIdTest, AssignmentIsIdempotentAndQueriesDoNotCreateIdentity)
{
    entt::registry actors;
    actor_net_id::Install(actors);
    actor_net_id::Install(actors);
    const auto assigned = actors.create();
    const auto untouched = actors.create();
    const auto id = actor_net_id::Assign(actors, assigned);
    ASSERT_NE(0, id);
    EXPECT_EQ(id, actor_net_id::Assign(actors, assigned));
    EXPECT_EQ(assigned, actor_net_id::Resolve(actors, id));
    EXPECT_EQ(id, actor_net_id::Of(actors, assigned));
    EXPECT_EQ(0, actor_net_id::Of(actors, untouched));
    EXPECT_TRUE(actor_net_id::Resolve(actors, 0) == entt::null);
    EXPECT_TRUE(actor_net_id::Resolve(actors, id + 1000000) == entt::null);
    EXPECT_EQ(0, actor_net_id::Of(actors, untouched));
}

TEST_F(ActorNetIdTest, RecycledEntityVersionNeverResolvesThroughPreviousNetworkId)
{
    entt::registry actors;
    actor_net_id::Install(actors);
    const auto before = actors.create();
    const auto oldId = actor_net_id::Assign(actors, before);
    ASSERT_NE(0, oldId);
    actors.destroy(before);
    const auto after = actors.create();
    ASSERT_EQ(entt::entt_traits<entt::entity>::to_entity(before),
              entt::entt_traits<entt::entity>::to_entity(after));
    ASSERT_NE(before, after);
    EXPECT_TRUE(actor_net_id::Resolve(actors, oldId) == entt::null);
    EXPECT_EQ(0, actor_net_id::Of(actors, before));
    const auto newId = actor_net_id::Assign(actors, after);
    ASSERT_NE(0, newId);
    EXPECT_NE(oldId, newId);
    EXPECT_EQ(after, actor_net_id::Resolve(actors, newId));
    EXPECT_EQ(0, actor_net_id::Assign(actors, before));
    EXPECT_EQ(newId, actor_net_id::Of(actors, after));
}

TEST_F(ActorNetIdTest, ClearAndDestructionDropMappingsWithoutReusingNetworkIds)
{
    entt::registry actors;
    actor_net_id::Install(actors);
    const auto actor = actors.create();
    const auto first = actor_net_id::Assign(actors, actor);
    ASSERT_NE(0, first);
    actors.destroy(actor);
    EXPECT_TRUE(actor_net_id::Resolve(actors, first) == entt::null);
    const auto recreated = actors.create();
    EXPECT_EQ(0, actor_net_id::Of(actors, recreated));
    const auto second = actor_net_id::Assign(actors, recreated);
    ASSERT_NE(0, second);
    EXPECT_NE(first, second);
    actors.clear();
    EXPECT_TRUE(actor_net_id::Resolve(actors, second) == entt::null);
    actor_net_id::Install(actors);
    const auto replacement = actors.create();
    const auto third = actor_net_id::Assign(actors, replacement);
    EXPECT_NE(0, third);
    EXPECT_NE(first, third);
    EXPECT_NE(second, third);
    EXPECT_EQ(replacement, actor_net_id::Resolve(actors, third));
}

TEST_F(ActorNetIdTest, DifferentNodesAndRegistriesDoNotAcceptEachOthersIdentities)
{
    entt::registry firstActors;
    entt::registry secondActors;
    actor_net_id::Install(firstActors);
    actor_net_id::Install(secondActors);
    const auto firstActor = firstActors.create();
    const auto secondActor = secondActors.create();
    const auto firstId = actor_net_id::Assign(firstActors, firstActor);
    tlsIdGeneratorManager.SetNodeId(102);
    const auto secondId = actor_net_id::Assign(secondActors, secondActor);
    ASSERT_NE(0, firstId);
    ASSERT_NE(0, secondId);
    EXPECT_EQ(101u, firstId >> 32);
    EXPECT_EQ(102u, secondId >> 32);
    EXPECT_NE(firstId, secondId);
    EXPECT_TRUE(actor_net_id::Resolve(firstActors, secondId) == entt::null);
    EXPECT_TRUE(actor_net_id::Resolve(secondActors, firstId) == entt::null);
    EXPECT_EQ(firstActor, actor_net_id::Resolve(firstActors, firstId));
    EXPECT_EQ(secondActor, actor_net_id::Resolve(secondActors, secondId));
}

TEST_F(ActorNetIdTest, ExhaustedSequenceStaysRefusedAndDoesNotWrap)
{
    // 缩小序号段覆盖真实号源的耗尽契约，避免遍历 2^32 个网络身份。
    TransientNodeCompositeIdGenerator<uint16_t, 2> generator;
    generator.set_node_id(3);
    uint16_t value = 0;
    for (uint16_t sequence = 1; sequence <= 3; ++sequence) {
        ASSERT_TRUE(generator.TryGenerateWithoutReuse(value));
        EXPECT_EQ((uint16_t{3} << 2) | sequence, value);
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
        value = 99;
        EXPECT_FALSE(generator.TryGenerateWithoutReuse(value));
        EXPECT_EQ(0, value);
    }
    generator.set_node_id(4);
    EXPECT_FALSE(generator.TryGenerateWithoutReuse(value)) << "改节点前缀不能清除本进程已经耗尽的序号";
}
