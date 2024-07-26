#include <gtest/gtest.h>
#include <boost/circular_buffer.hpp>
#include "muduo/base/Timestamp.h"
#include "type_define/type_define.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"
#include "system/player_change_scene.h"
#include "system/scene/scene_system.h"

EntityVector globalSceneList;

entt::entity CreateMainSceneNodeEntity()
{
	// Create a main scene node entity
	const auto sceneEntity = tls.registry.create();
	AddMainSceneNodeComponent(tls.registry, sceneEntity);
	return sceneEntity;
}

entt::entity CreatePlayerEntity()
{
	// Create a player entity and initialize change scene queue
	auto playerEntity = tls.registry.create();
	PlayerChangeSceneSystem::InitChangeSceneQueue(playerEntity);
	return playerEntity;
}

CentreChangeSceneInfo& GetPlayerFrontChangeSceneInfo(entt::entity playerEntity)
{
	// Get the front change scene info for the player
	return tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.front();
}

TEST(PlayerChangeScene, CreateMainScene)
{
	// Test creating main scene nodes
	const auto mainSceneNode = CreateMainSceneNodeEntity();
	SceneInfo sceneInfo;
	for (uint32_t i = 0; i < 10; ++i)
	{
		sceneInfo.set_scene_confid(i);
		for (uint32_t j = 0; j < 2; ++j)
		{
			globalSceneList.push_back(ScenesSystem::CreateScene2GameNode({ .node = mainSceneNode, .sceneInfo = sceneInfo }));
		}
	}
}

TEST(PlayerChangeScene, QueueFull)
{
	// Test pushing change scene info when queue is full
	const auto playerEntity = CreatePlayerEntity();
	for (uint8_t i = 0; i < kMaxChangeSceneQueue; ++i)
	{
		CentreChangeSceneInfo changeInfo;
		EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeInfo));
	}
	CentreChangeSceneInfo changeInfo;
	EXPECT_EQ(kRetEnterSceneChangingGs, PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeInfo));
}

// Test: Directly switch within the same game server, should succeed directly
TEST(PlayerChangeScene, ChangeSameGsSceneNotEnqueue)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	ScenesSystem::EnterScene({ fromSceneEntity, playerEntity });
	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eSameGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, Gs1SceneToGs2SceneInZoneServer)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	ScenesSystem::EnterScene({ fromSceneEntity, playerEntity });

	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs); // todo scene logic
	changeInfo.set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeInfo));

	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(!tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eEnterGsSceneSucceed);

	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(!tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, DiffGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	ScenesSystem::EnterScene({ fromSceneEntity, playerEntity });

	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(!tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, SameGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	ScenesSystem::EnterScene({ fromSceneEntity, playerEntity });

	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eSameGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, CrossServerDiffGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	ScenesSystem::EnterScene({ fromSceneEntity, playerEntity });

	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_FALSE(tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());
}

// Test various states
TEST(PlayerChangeScene, ServerCrush)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	ScenesSystem::EnterScene({ fromSceneEntity, playerEntity });

	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_FALSE(tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
	PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity); // crash
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());

	ScenesSystem::EnterScene({ fromSceneEntity, playerEntity });
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
	EXPECT_EQ(kOK, PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_FALSE(tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<PlayerCentreChangeSceneQueue>(playerEntity).change_scene_queue_.empty());
}

int32_t main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
