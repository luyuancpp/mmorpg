#include <gtest/gtest.h>

#include <iterator>
#include <thread>
#include <type_traits>

#include "thread_context/ecs_context.h"
#include "type_alias/player_session_type_alias.h"

namespace
{
static_assert(!std::is_convertible_v<entt::entity, SceneEntity>);
static_assert(!std::is_convertible_v<SceneEntity, entt::entity>);

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

TEST_F(SessionPlayerIndexTest, EmptyLookupDoesNotCreateComponentPools)
{
    const auto& registry = tlsEcs.actorRegistry;
    const auto poolsBefore = std::distance(registry.storage().begin(), registry.storage().end());
    const auto& scenes = tlsEcs.sceneRegistry;
    const auto scenePoolsBefore = std::distance(scenes.storage().begin(), scenes.storage().end());

    EXPECT_FALSE(SessionMap().contains(17));

    EXPECT_EQ(poolsBefore, std::distance(registry.storage().begin(), registry.storage().end()));
    EXPECT_EQ(scenePoolsBefore, std::distance(scenes.storage().begin(), scenes.storage().end()));
}

TEST_F(SessionPlayerIndexTest, SurvivesUnrelatedRegistryClear)
{
    constexpr SessionId session = 17;
    constexpr Guid player = 100000000000001ULL;
    SessionMap().insert_or_assign(session, player);

    // 会话路由的寿命属于上下文，不由场景或角色组件池的清理决定。
    tlsEcs.actorRegistry.clear();
    tlsEcs.sceneRegistry.clear();

    ASSERT_TRUE(SessionMap().contains(session));
    EXPECT_EQ(player, SessionMap().at(session));
}

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

TEST_F(SessionPlayerIndexTest, RebindingSessionReplacesOnlyItsPlayer)
{
    SessionMap().insert_or_assign(17, 1001);
    SessionMap().insert_or_assign(18, 1002);

    SessionMap().insert_or_assign(17, 1003);

    EXPECT_EQ(2U, SessionMap().size());
    EXPECT_EQ(1003U, SessionMap().at(17));
    EXPECT_EQ(1002U, SessionMap().at(18));
    SessionMap().erase(17);
    EXPECT_EQ(1002U, SessionMap().at(18));
}

TEST_F(SessionPlayerIndexTest, SeparateLoopThreadsOwnSeparateIndexes)
{
    SessionMap().insert_or_assign(17, 1001);
    bool workerStartsEmpty = false;
    Guid workerPlayer = 0;
    std::thread worker([&] {
        workerStartsEmpty = SessionMap().empty();
        SessionMap().insert_or_assign(17, 1002);
        workerPlayer = SessionMap().at(17);
        tlsEcs.Clear();
    });
    worker.join();

    EXPECT_TRUE(workerStartsEmpty);
    EXPECT_EQ(1002U, workerPlayer);
    EXPECT_EQ(1001U, SessionMap().at(17));
}
} // namespace
