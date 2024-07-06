#include <gtest/gtest.h>

#include <boost/circular_buffer.hpp>

#include "muduo/base/Timestamp.h"

#include "type_define/type_define.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"
#include "system/player_change_scene.h"
#include "system/scene/scene_system.h"

EntityVector scene_list;

entt::entity CreateMainSceneNode()
{
	const auto scene_entity = tls.registry.create();
	AddMainSceneNodeComponent(tls.registry, scene_entity);
	return scene_entity;
}

entt::entity CreatePlayer()
{
	auto player = tls.registry.create();
	PlayerChangeSceneSystem::InitChangeSceneQueue(player);
    return player;
}

CentreChangeSceneInfo& GetPlayerFrontChangeSceneInfo(entt::entity player)
{
    return tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.front();
}

TEST(PlayerChangeScene, CreateMainScene)
{
    const auto node = CreateMainSceneNode();
    SceneInfo scene_info;
    for (uint32_t i = 0; i < 10; ++i)
    {
        scene_info.set_scene_confid(i);
        for (uint32_t j = 0; j < 2; ++j)
        {
            scene_list.push_back(ScenesSystem::CreateScene2GameNode({.node = node, .scene_info = scene_info }));
        }
    }
}

TEST(PlayerChangeScene, QueueFull)
{
    const auto player = CreatePlayer();
    for (uint8_t i = 0; i < kMaxChangeSceneQueue; ++i)
    {
        CentreChangeSceneInfo info;
        EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, info));
    }
    CentreChangeSceneInfo info;
    EXPECT_EQ(kRetEnterSceneChangingGs, PlayerChangeSceneSystem::PushChangeSceneInfo(player, info));
}

//1:同一个gs直接切，队列直接成功
TEST(PlayerChangeScene, ChangeSameGsSceneNotEnqueue)
{
    const auto player = CreatePlayer();
    const auto from_scene = *(scene_list.begin()++);
    const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene).guid();
    ScenesSystem::EnterScene({from_scene, player});
    CentreChangeSceneInfo change_info;
    change_info.set_guid(scene_id);
    change_info.set_change_gs_type(CentreChangeSceneInfo::eSameGs);//todo scene logic
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, Gs1SceneToGs2SceneInZoneServer)
{
    const auto player = CreatePlayer();
    const auto from_scene = *(scene_list.begin()++);
    const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene).guid();
    ScenesSystem::EnterScene({from_scene, player});
    CentreChangeSceneInfo change_info;
    change_info.set_guid(scene_id);
    change_info.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);//todo scene logic
    change_info.set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(!tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(CentreChangeSceneInfo::eEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(!tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, DiffGs)
{
    const auto player = CreatePlayer();
    const auto from_scene = *(scene_list.begin()++);
    const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene).guid();
    ScenesSystem::EnterScene({from_scene, player});

    CentreChangeSceneInfo change_info;
    change_info.set_guid(scene_id);
    change_info.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(!tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
}


TEST(PlayerChangeScene, SameGs)
{
    const auto player = CreatePlayer();
    const auto from_scene = *(scene_list.begin()++);
    const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene).guid();
    ScenesSystem::EnterScene({from_scene, player});

    CentreChangeSceneInfo change_info;
    change_info.set_guid(scene_id);
    change_info.set_change_gs_type(CentreChangeSceneInfo::eSameGs);//todo scene logic
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));    
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, CrossServerDiffGs)
{
    const auto player = CreatePlayer();
    const auto from_scene = *(scene_list.begin()++);
    const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene).guid();
    ScenesSystem::EnterScene({from_scene, player});

    CentreChangeSceneInfo change_info;
    change_info.set_guid(scene_id);
    change_info.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);//todo scene logic
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
}

//测试各种状态
TEST(PlayerChangeScene, ServerCrush)
{
    const auto player = CreatePlayer();
    const auto from_scene = *(scene_list.begin()++);
    const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene).guid();
    ScenesSystem::EnterScene({from_scene, player});

    CentreChangeSceneInfo change_info;
    change_info.set_guid(scene_id);
    change_info.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);//todo scene logic
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
    PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);//crash
    EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());

    ScenesSystem::EnterScene({from_scene, player});
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_info));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_FALSE(tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
    GetPlayerFrontChangeSceneInfo(player).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player).change_scene_queue_.empty());
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
