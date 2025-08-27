#include <gtest/gtest.h>
#include <boost/circular_buffer.hpp>
#include "muduo/base/Timestamp.h"
#include "base/core/type_define/type_define.h"

#include "proto/table/scene_error_tip.pb.h"
#include "proto/table/common_error_tip.pb.h"
#include "scene/system/player_change_scene_system.h"
#include "scene/system/scene_system.h"

EntityVector globalSceneList;

extern thread_local RegistryManager tlsRegistryManager;

entt::entity CreateMainSceneNodeEntity()
{
	// Create a main scene node entity
	const auto sceneEntity = tlsRegistryManager.actorRegistry.create();
	AddMainSceneNodeComponent(tlsRegistryManager.actorRegistry, sceneEntity);
	return sceneEntity;
}

entt::entity CreatePlayerEntity()
{
	// Create a player entity and initialize change scene queue
	auto playerEntity = tlsRegistryManager.actorRegistry.create();
	return playerEntity;
}

ChangeSceneInfoPBComponent& GetPlayerFrontChangeSceneInfo(entt::entity playerEntity)
{
	// Get the front change scene info for the player
	return *tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).front();
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

// Test: Directly switch within the same game server, should succeed directly
TEST(PlayerChangeScene, ChangeSameGsSceneNotEnqueue)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });
	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eSameGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

TEST(PlayerChangeScene, Gs1SceneToGs2SceneInZoneServer)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs); // todo scene logic
	changeInfo.set_state(ChangeSceneInfoPBComponent::ePendingLeave);
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));

	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeSceneInfoPBComponent::eWaitingEnter);
	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeSceneInfoPBComponent::eEnterSucceed);

	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());

	PlayerChangeSceneUtil::OnTargetSceneNodeEnterComplete(playerEntity);

	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

TEST(PlayerChangeScene, DiffGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs);
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeSceneInfoPBComponent::eEnterSucceed);
	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());

	PlayerChangeSceneUtil::OnTargetSceneNodeEnterComplete(playerEntity);

	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

TEST(PlayerChangeScene, SameGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eSameGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

TEST(PlayerChangeScene, CrossServerDiffGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeSceneInfoPBComponent::eEnterSucceed);
	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());

	PlayerChangeSceneUtil::OnTargetSceneNodeEnterComplete(playerEntity);
	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

// Test various states
TEST(PlayerChangeScene, ServerCrush)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	ChangeSceneInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeSceneInfoPBComponent::eDifferentGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeSceneInfoPBComponent::eWaitingEnter);
	PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity); // crash
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());

	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });
	EXPECT_EQ(kSuccess, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeSceneInfoPBComponent::eWaitingEnter);
	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());

	PlayerChangeSceneUtil::OnTargetSceneNodeEnterComplete(playerEntity);
	PlayerChangeSceneUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

int32_t main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
