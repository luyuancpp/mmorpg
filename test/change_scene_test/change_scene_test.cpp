#include <gtest/gtest.h>
#include <boost/circular_buffer.hpp>
#include "muduo/base/Timestamp.h"
#include "type_define/type_define.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"
#include "system/player_change_scene.h"
#include "system/scene/scene_system.h"

EntityVector global_scene_list;

entt::entity CreateMainSceneNodeEntity()
{
	// Create a main scene node entity
	const auto main_scene_entity = tls.registry.create();
	AddMainSceneNodeComponent(tls.registry, main_scene_entity);
	return main_scene_entity;
}

entt::entity CreatePlayerEntity()
{
	// Create a player entity and initialize change scene queue
	auto player_entity = tls.registry.create();
	PlayerChangeSceneSystem::InitChangeSceneQueue(player_entity);
	return player_entity;
}

CentreChangeSceneInfo& GetPlayerFrontChangeSceneInfo(entt::entity player_entity)
{
	// Get the front change scene info for the player
	return tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.front();
}

TEST(PlayerChangeScene, CreateMainScene)
{
	// Test creating main scene nodes
	const auto main_scene_node = CreateMainSceneNodeEntity();
	SceneInfo scene_info;
	for (uint32_t i = 0; i < 10; ++i)
	{
		scene_info.set_scene_confid(i);
		for (uint32_t j = 0; j < 2; ++j)
		{
			global_scene_list.push_back(ScenesSystem::CreateScene2GameNode({ .node = main_scene_node, .sceneInfo = scene_info }));
		}
	}
}

TEST(PlayerChangeScene, QueueFull)
{
	// Test pushing change scene info when queue is full
	const auto player_entity = CreatePlayerEntity();
	for (uint8_t i = 0; i < kMaxChangeSceneQueue; ++i)
	{
		CentreChangeSceneInfo change_info;
		EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player_entity, change_info));
	}
	CentreChangeSceneInfo change_info;
	EXPECT_EQ(kRetEnterSceneChangingGs, PlayerChangeSceneSystem::PushChangeSceneInfo(player_entity, change_info));
}

// Test: Directly switch within the same game server, should succeed directly
TEST(PlayerChangeScene, ChangeSameGsSceneNotEnqueue)
{
	const auto player_entity = CreatePlayerEntity();
	const auto from_scene_entity = *(global_scene_list.begin()++);
	const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene_entity).guid();
	ScenesSystem::EnterScene({ from_scene_entity, player_entity });
	CentreChangeSceneInfo change_info;
	change_info.set_guid(scene_id);
	change_info.set_change_gs_type(CentreChangeSceneInfo::eSameGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player_entity, change_info));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, Gs1SceneToGs2SceneInZoneServer)
{
	const auto player_entity = CreatePlayerEntity();
	const auto from_scene_entity = *(global_scene_list.begin()++);
	const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene_entity).guid();
	ScenesSystem::EnterScene({ from_scene_entity, player_entity });
	CentreChangeSceneInfo change_info;
	change_info.set_guid(scene_id);
	change_info.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs); // todo scene logic
	change_info.set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player_entity, change_info));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_TRUE(!tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
	GetPlayerFrontChangeSceneInfo(player_entity).set_change_gs_status(CentreChangeSceneInfo::eEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_TRUE(!tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
	GetPlayerFrontChangeSceneInfo(player_entity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, DiffGs)
{
	const auto player_entity = CreatePlayerEntity();
	const auto from_scene_entity = *(global_scene_list.begin()++);
	const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene_entity).guid();
	ScenesSystem::EnterScene({ from_scene_entity, player_entity });

	CentreChangeSceneInfo change_info;
	change_info.set_guid(scene_id);
	change_info.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player_entity, change_info));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_TRUE(!tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
	GetPlayerFrontChangeSceneInfo(player_entity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, SameGs)
{
	const auto player_entity = CreatePlayerEntity();
	const auto from_scene_entity = *(global_scene_list.begin()++);
	const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene_entity).guid();
	ScenesSystem::EnterScene({ from_scene_entity, player_entity });

	CentreChangeSceneInfo change_info;
	change_info.set_guid(scene_id);
	change_info.set_change_gs_type(CentreChangeSceneInfo::eSameGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player_entity, change_info));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, CrossServerDiffGs)
{
	const auto player_entity = CreatePlayerEntity();
	const auto from_scene_entity = *(global_scene_list.begin()++);
	const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene_entity).guid();
	ScenesSystem::EnterScene({ from_scene_entity, player_entity });

	CentreChangeSceneInfo change_info;
	change_info.set_guid(scene_id);
	change_info.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player_entity, change_info));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_FALSE(tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
	GetPlayerFrontChangeSceneInfo(player_entity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
}

// Test various states
TEST(PlayerChangeScene, ServerCrush)
{
	const auto player_entity = CreatePlayerEntity();
	const auto from_scene_entity = *(global_scene_list.begin()++);
	const auto scene_id = tls.scene_registry.get<SceneInfo>(from_scene_entity).guid();
	ScenesSystem::EnterScene({ from_scene_entity, player_entity });

	CentreChangeSceneInfo change_info;
	change_info.set_guid(scene_id);
	change_info.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player_entity, change_info));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_FALSE(tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
	GetPlayerFrontChangeSceneInfo(player_entity).set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
	PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player_entity); // crash
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());

	ScenesSystem::EnterScene({ from_scene_entity, player_entity });
	GetPlayerFrontChangeSceneInfo(player_entity).set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(player_entity, change_info));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_FALSE(tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
	GetPlayerFrontChangeSceneInfo(player_entity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player_entity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(player_entity).change_scene_queue_.empty());
}

int32_t main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
