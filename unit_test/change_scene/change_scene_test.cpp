#include <gtest/gtest.h>

#include <boost/circular_buffer.hpp>

#include "muduo/base/Timestamp.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/tips_id.h"
#include "src/system/player_change_scene.h"
#include "src/game_logic/scene/scene_system.h"

ScenesSystem sm;
EntityVector scene_list;

entt::entity CreateMainSceneNode()
{
	const auto scene_entity = tls.registry.create();
	AddMainSceneNodeComponent(scene_entity);
	return scene_entity;
}

entt::entity CreatePlayer()
{
	auto player = tls.registry.create();
	PlayerChangeSceneSystem::InitChangeSceneQueue(player);
    return player;
}

ControllerChangeSceneInfo& GetPlayerFrontChangeSceneInfo(entt::entity player)
{
    return tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.front();
}

TEST(PlayerChangeScene, CreateMainScene)
{
    const auto node = CreateMainSceneNode();
    for (uint32_t i = 0; i < 10; ++i)
    {
        for (uint32_t j = 0; j < 2; ++j)
        {
            scene_list.push_back(sm.CreateScene2GameNode({.node_ = node, .scene_config_id_ = i}));
        }
    }
}

TEST(PlayerChangeScene, QueueFull)
{
    auto player = CreatePlayer();
    for (uint8_t i = 0; i < PlayerControllerChangeSceneQueue::kMaxChangeSceneQueue; ++i)
    {
        ControllerChangeSceneInfo info;
        EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, info));
    }
    ControllerChangeSceneInfo info;
    EXPECT_EQ(kRetEnterSceneChangingGs, PlayerChangeSceneSystem::PushChangeSceneInfo(player, info));
}

//1:同一个gs直接切，队列直接成功
TEST(PlayerChangeScene, ChangeSameGsSceneNotEnqueue)
{
    const auto player = CreatePlayer();
    const auto scene_id = tls.registry.get<SceneInfo>(*(scene_list.begin()++)).guid();
    const auto from_scene = ScenesSystem::GetSceneByGuid(scene_id);
    ScenesSystem::EnterScene({from_scene, player});
    ControllerChangeSceneInfo change_info;
    change_info.mutable_scene_info()->set_guid(scene_id);
    change_info.set_change_gs_type(ControllerChangeSceneInfo::eSameGs);//todo scene logic
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, Gs1SceneToGs2SceneInZoneServer)
{
    const auto player = CreatePlayer();
    const auto scene_id = tls.registry.get<SceneInfo>(*(scene_list.begin()++)).guid();
    const auto from_scene = ScenesSystem::GetSceneByGuid(scene_id);
    ScenesSystem::EnterScene({from_scene, player});
    ControllerChangeSceneInfo change_info;
    change_info.mutable_scene_info()->set_guid(scene_id);
    change_info.set_change_gs_type(ControllerChangeSceneInfo::eDifferentGs);//todo scene logic
    change_info.set_change_gs_status(ControllerChangeSceneInfo::eLeaveGsScene);
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(!tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(ControllerChangeSceneInfo::eEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(!tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(ControllerChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, NormalServerGs2CrossServerGs)
{
    const auto player = CreatePlayer();
    const auto scene_id = tls.registry.get<SceneInfo>(*(scene_list.begin()++)).guid();
    const auto from_scene = ScenesSystem::GetSceneByGuid(scene_id);
    ScenesSystem::EnterScene({from_scene, player});

    ControllerChangeSceneInfo change_info;
    change_info.mutable_scene_info()->set_guid(scene_id);
    change_info.set_change_cross_server_type(ControllerChangeSceneInfo::eCrossServer);
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(!tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_type(ControllerChangeSceneInfo::eDifferentGs);
    GetPlayerFrontChangeSceneInfo(player).set_change_cross_server_status(ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(!tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(ControllerChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
}


TEST(PlayerChangeScene, CrossServerSameGs)
{
    const auto player = CreatePlayer();
    const auto scene_id = tls.registry.get<SceneInfo>(*(scene_list.begin()++)).guid();
    const auto from_scene = ScenesSystem::GetSceneByGuid(scene_id);
    ScenesSystem::EnterScene({from_scene, player});

    ControllerChangeSceneInfo change_info;
    change_info.mutable_scene_info()->set_guid(scene_id);
    change_info.set_change_cross_server_type(ControllerChangeSceneInfo::eCrossServer);
    change_info.set_change_gs_type(ControllerChangeSceneInfo::eSameGs);//todo scene logic
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));    
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_cross_server_status(ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, CrossServerDiffGs)
{
    const auto player = CreatePlayer();
    const auto scene_id = tls.registry.get<SceneInfo>(*(scene_list.begin()++)).guid();
    const auto from_scene = ScenesSystem::GetSceneByGuid(scene_id);
    ScenesSystem::EnterScene({from_scene, player});

    ControllerChangeSceneInfo change_info;
    change_info.mutable_scene_info()->set_guid(scene_id);
    change_info.set_change_cross_server_type(ControllerChangeSceneInfo::eCrossServer);
    change_info.set_change_gs_type(ControllerChangeSceneInfo::eDifferentGs);//todo scene logic
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_cross_server_status(ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(ControllerChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, CrossServerGs2NormalServerGs)
{
    const auto player = CreatePlayer();
    const auto scene_id = tls.registry.get<SceneInfo>(*(scene_list.begin()++)).guid();
    const auto from_scene = ScenesSystem::GetSceneByGuid(scene_id);
    ScenesSystem::EnterScene({from_scene, player});

    ControllerChangeSceneInfo change_info;
    change_info.mutable_scene_info()->set_guid(scene_id);
    change_info.set_change_cross_server_type(ControllerChangeSceneInfo::eCrossServer);
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_type(ControllerChangeSceneInfo::eDifferentGs);
    GetPlayerFrontChangeSceneInfo(player).set_change_cross_server_status(ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(ControllerChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
}


//测试各种状态
TEST(PlayerChangeScene, ServerCrush)
{
    const auto player = CreatePlayer();
    const auto scene_id = tls.registry.get<SceneInfo>(*(scene_list.begin()++)).guid();
    const auto from_scene = ScenesSystem::GetSceneByGuid(scene_id);
    ScenesSystem::EnterScene({from_scene, player});

    ControllerChangeSceneInfo change_info;
    change_info.mutable_scene_info()->set_guid(scene_id);
    change_info.set_change_cross_server_type(ControllerChangeSceneInfo::eCrossServer);
    change_info.set_change_gs_type(ControllerChangeSceneInfo::eDifferentGs);//todo scene logic
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_cross_server_status(ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(ControllerChangeSceneInfo::eLeaveGsScene);
    PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);//crash
    EXPECT_TRUE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());

    ScenesSystem::EnterScene({from_scene, player});
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(ControllerChangeSceneInfo::eLeaveGsScene);
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_type(ControllerChangeSceneInfo::eDifferentGs);
    GetPlayerFrontChangeSceneInfo(player).set_change_cross_server_status(ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(ControllerChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerControllerChangeSceneQueue>(player).change_scene_queue_.empty());
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
