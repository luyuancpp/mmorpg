#include <gtest/gtest.h>

#include <iterator>
#include <type_traits>

#include "thread_context/ecs_context.h"
#include "type_alias/player_session_type_alias.h"
#include "utils/registry/describe_entity.h"

namespace
{
static_assert(!std::is_convertible_v<entt::entity, SceneEntity>);
static_assert(!std::is_convertible_v<SceneEntity, entt::entity>);

struct DescribeProbeTag {};
struct DescribeOtherProbe { int value; };
struct DescribeSceneProbe { int value; };

TEST(DescribeEntityTest, ListsExactlyTheAttachedComponents)
{
    entt::registry registry;
    const auto actor = registry.create();
    registry.emplace<PlayerGuidComp>(actor, PlayerGuidComp{42});
    registry.emplace<DescribeProbeTag>(actor);
    registry.emplace<DescribeOtherProbe>(registry.create(), 7);

    const auto description = DescribeEntity(registry, actor);

    EXPECT_EQ(0U, description.find("entity="));
    EXPECT_NE(std::string::npos, description.find("valid=1"));
    EXPECT_NE(std::string::npos, description.find("PlayerGuidComp"));
    EXPECT_NE(std::string::npos, description.find("DescribeProbeTag"));
    EXPECT_EQ(std::string::npos, description.find("DescribeOtherProbe"));
}

TEST(DescribeEntityTest, StaleAndNullHandlesAreSafe)
{
    entt::registry registry;
    const auto actor = registry.create();
    registry.emplace<PlayerGuidComp>(actor, PlayerGuidComp{42});
    registry.destroy(actor);
    registry.emplace<DescribeProbeTag>(registry.create());
    const auto poolCount = std::distance(registry.storage().begin(), registry.storage().end());

    EXPECT_NO_THROW({ EXPECT_NE(std::string::npos, DescribeEntity(registry, actor).find("valid=0")); });
    EXPECT_NE(std::string::npos, DescribeEntity(registry, entt::entity{entt::null}).find("valid=0"));
    EXPECT_EQ(poolCount, std::distance(registry.storage().begin(), registry.storage().end()));
}

TEST(DescribeEntityTest, WorksForSceneRegistry)
{
    tlsEcs.Clear();
    const auto scene = tlsEcs.sceneRegistry.create();
    tlsEcs.sceneRegistry.emplace<DescribeSceneProbe>(scene, 7);
    EXPECT_NE(std::string::npos, DescribeEntity(tlsEcs.sceneRegistry, scene).find("DescribeSceneProbe"));
    tlsEcs.Clear();
}

TEST(EcsContextTest, GetPlayerIdReadsGuidOrZero)
{
    EcsContext context;
    const auto actor = context.actorRegistry.create();
    context.actorRegistry.emplace<PlayerGuidComp>(actor, PlayerGuidComp{42});
    EXPECT_EQ(42U, context.GetPlayerId(actor));
    EXPECT_EQ(0U, context.GetPlayerId(context.actorRegistry.create()));
    context.actorRegistry.destroy(actor);
    const auto replacement = context.actorRegistry.create();
    context.actorRegistry.emplace<PlayerGuidComp>(replacement, PlayerGuidComp{7});
    EXPECT_EQ(entt::to_entity(actor), entt::to_entity(replacement));
    EXPECT_EQ(0U, context.GetPlayerId(actor));
    EXPECT_EQ(7U, context.GetPlayerId(replacement));
    EXPECT_EQ(0U, context.GetPlayerId(entt::null));
}

TEST(SceneEntityContract, SceneAndActorLifetimesAreIndependent)
{
    EcsContext context;
    const auto actor = context.actorRegistry.create();
    const auto scene = context.sceneRegistry.create();
    static_assert(std::is_same_v<std::remove_cv_t<decltype(scene)>, SceneEntity>);
    context.sceneRegistry.destroy(scene);
    EXPECT_TRUE(context.actorRegistry.valid(actor));
    EXPECT_FALSE(context.sceneRegistry.valid(scene));
    const auto replacement = context.sceneRegistry.create();
    EXPECT_NE(scene, replacement);
    EXPECT_FALSE(context.sceneRegistry.valid(scene));
}

class SessionPlayerIndexTest : public testing::Test
{
protected:
    void SetUp() override { tlsEcs.Clear(); }
    void TearDown() override { tlsEcs.Clear(); }
};

TEST_F(SessionPlayerIndexTest, ContextClearDropsBothIndexesAndAllowsNewSession)
{
    constexpr SessionId session = 17;
    constexpr Guid player = 100000000000001ULL;
    const auto actor = tlsEcs.actorRegistry.create();
    tlsEcs.playerList.emplace(player, actor);
    SessionMap().insert_or_assign(session, player);

    tlsEcs.Clear();

    EXPECT_TRUE(SessionMap().empty());
    EXPECT_TRUE(tlsEcs.GetPlayer(player) == entt::null);
    EXPECT_FALSE(tlsEcs.actorRegistry.valid(actor));

    SessionMap().insert_or_assign(session, player + 1);
    EXPECT_EQ(player + 1, SessionMap().at(session));
}

} // namespace
