#include <gtest/gtest.h>
#include <boost/circular_buffer.hpp>
#include "muduo/base/Timestamp.h"
#include "type_define/type_define.h"
#include "thread_local/storage.h"
#include "pbc/scene_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"
#include "scene/system/player_change_scene_system.h"
#include "scene/system/scene_system.h"

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
	return playerEntity;
}

ChangeSceneInfoPBComponent& GetPlayerFrontChangeSceneInfo(entt::entity playerEntity)
{
	// Get the front change scene info for the player
	return tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.front();
}

TEST(PlayerChangeScene, CreateMainScene)
{
	// Test creating main scene nodes
	const auto mainSceneNode = CreateMainSceneNodeEntity();
	SceneInfoPBComponent sceneInfo;
	for (uint32_t i = 0; i < 10; ++i)
	{
		sceneInfo.set_scene_confid(i);
		for (uint32_t j = 0; j < 2; ++j)
		{
			globalSceneList.push_back(SceneUtil::CreateSceneToSceneNode({ .node = mainSceneNode, .sceneInfo = sceneInfo }));
		}
	}
}

TEST(PlayerChangeScene, QueueFull)
{
	// Test pushing change scene info when queue is full
	const auto playerEntity = CreatePlayerEntity();
	for (uint8_t i = 0; i < kMaxChangeSceneQueue; ++i)
	{
		ChangeSceneInfoPBComponent changeInfo;
		EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	}
	ChangeSceneInfoPBComponent changeInfo;
	EXPECT_EQ(kEnterSceneChangingGs, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
}

// Test: Directly switch within the same game server, should succeed directly
TEST(PlayerChangeScene, ChangeSameGsSceneNotEnqueue)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });
	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eSameGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());
}

TEST(PlayerChangeScene, Gs1SceneToGs2SceneInZoneServer)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs); // todo scene logic
	changeInfo.set_change_gs_status(ChangeSceneInfoPBComponent::eLeaveGsScene);
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));

	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(!tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(ChangeSceneInfoPBComponent::eEnterGsSceneSucceed);

	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(!tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(ChangeSceneInfoPBComponent::eGateEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());
}

TEST(PlayerChangeScene, DiffGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs);
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(!tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(ChangeSceneInfoPBComponent::eGateEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());
}

TEST(PlayerChangeScene, SameGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eSameGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());
}

TEST(PlayerChangeScene, CrossServerDiffGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_FALSE(tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(ChangeSceneInfoPBComponent::eGateEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());
}

// Test various states
TEST(PlayerChangeScene, ServerCrush)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_FALSE(tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(ChangeSceneInfoPBComponent::eLeaveGsScene);
	PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity); // crash
	EXPECT_TRUE(tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());

	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(ChangeSceneInfoPBComponent::eLeaveGsScene);
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_FALSE(tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(ChangeSceneInfoPBComponent::eGateEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<ChangeSceneQueuePBComponent>(playerEntity).changeSceneQueue.empty());
}

int32_t main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
