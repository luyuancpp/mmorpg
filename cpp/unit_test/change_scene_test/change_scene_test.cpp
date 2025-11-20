#include <gtest/gtest.h>
#include <boost/circular_buffer.hpp>
#include "muduo/base/Timestamp.h"
#include "engine/core/type_define/type_define.h"

#include "table/proto/tip/scene_error_tip.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "scene/system/player_change_room.h"
#include <threading/registry_manager.h>
#include <modules/scene/system/room_common.h>
#include <scene/system/room.h>

EntityVector globalSceneList;

extern thread_local RegistryManager tlsRegistryManager;

entt::entity CreateMainSceneNodeEntity()
{
	// Create a main scene node entity
	const auto sceneEntity = tlsRegistryManager.actorRegistry.create();
	AddMainRoomToNodeComponent(tlsRegistryManager.actorRegistry, sceneEntity);
	return sceneEntity;
}

entt::entity CreatePlayerEntity()
{
	// Create a player entity and initialize change scene queue
	auto playerEntity = tlsRegistryManager.actorRegistry.create();
	return playerEntity;
}

ChangeRoomInfoPBComponent& GetPlayerFrontChangeSceneInfo(entt::entity playerEntity)
{
	// Get the front change scene info for the player
	return *tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).front();
}

TEST(PlayerChangeScene, CreateMainScene)
{
	// Test creating main scene nodes
	const auto mainSceneNode = CreateMainSceneNodeEntity();
	RoomInfoPBComponent sceneInfo;
	for (uint32_t i = 0; i < 10; ++i)
	{
		sceneInfo.set_scene_confid(i);
		for (uint32_t j = 0; j < 2; ++j)
		{
			globalSceneList.push_back(RoomCommon::CreateRoomOnRoomNode({ .node = mainSceneNode, .roomInfo = sceneInfo }));
		}
	}
}

// Test: Directly switch within the same game server, should succeed directly
TEST(PlayerChangeScene, ChangeSameGsSceneNotEnqueue)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(fromSceneEntity).guid();
	RoomCommon::EnterRoom({ fromSceneEntity, playerEntity });
	ChangeRoomInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeRoomInfoPBComponent::eSameGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeRoomUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

TEST(PlayerChangeScene, Gs1SceneToGs2SceneInZoneServer)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(fromSceneEntity).guid();
	RoomCommon::EnterRoom({ fromSceneEntity, playerEntity });

	ChangeRoomInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeRoomInfoPBComponent::eDifferentGs); // todo scene logic
	changeInfo.set_state(ChangeRoomInfoPBComponent::ePendingLeave);
	EXPECT_EQ(kSuccess, PlayerChangeRoomUtil::PushChangeSceneInfo(playerEntity, changeInfo));

	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeRoomInfoPBComponent::eWaitingEnter);
	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeRoomInfoPBComponent::eEnterSucceed);

	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());

	PlayerChangeRoomUtil::OnTargetSceneNodeEnterComplete(playerEntity);

	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

TEST(PlayerChangeScene, DiffGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(fromSceneEntity).guid();
	RoomCommon::EnterRoom({ fromSceneEntity, playerEntity });

	ChangeRoomInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeRoomInfoPBComponent::eDifferentGs);
	EXPECT_EQ(kSuccess, PlayerChangeRoomUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeRoomInfoPBComponent::eEnterSucceed);
	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());

	PlayerChangeRoomUtil::OnTargetSceneNodeEnterComplete(playerEntity);

	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

TEST(PlayerChangeScene, SameGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(fromSceneEntity).guid();
	RoomCommon::EnterRoom({ fromSceneEntity, playerEntity });

	ChangeRoomInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeRoomInfoPBComponent::eSameGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeRoomUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

TEST(PlayerChangeScene, CrossServerDiffGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(fromSceneEntity).guid();
	RoomCommon::EnterRoom({ fromSceneEntity, playerEntity });

	ChangeRoomInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeRoomInfoPBComponent::eDifferentGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeRoomUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeRoomInfoPBComponent::eEnterSucceed);
	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());

	PlayerChangeRoomUtil::OnTargetSceneNodeEnterComplete(playerEntity);
	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

// Test various states
TEST(PlayerChangeScene, ServerCrush)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(fromSceneEntity).guid();
	RoomCommon::EnterRoom({ fromSceneEntity, playerEntity });

	ChangeRoomInfoPBComponent changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(ChangeRoomInfoPBComponent::eDifferentGs); // todo scene logic
	EXPECT_EQ(kSuccess, PlayerChangeRoomUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeRoomInfoPBComponent::eWaitingEnter);
	PlayerChangeRoomUtil::PopFrontChangeSceneQueue(playerEntity); // crash
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());

	RoomCommon::EnterRoom({ fromSceneEntity, playerEntity });
	EXPECT_EQ(kSuccess, PlayerChangeRoomUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeRoomInfoPBComponent::eWaitingEnter);
	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_FALSE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_state(ChangeRoomInfoPBComponent::eEnterSucceed);

	PlayerChangeRoomUtil::OnTargetSceneNodeEnterComplete(playerEntity);
	PlayerChangeRoomUtil::ProgressSceneChangeState(playerEntity);
	EXPECT_TRUE(tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(playerEntity).empty());
}

int32_t main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
