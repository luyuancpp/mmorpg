#include <gtest/gtest.h>
#include <boost/circular_buffer.hpp>
#include "muduo/base/Timestamp.h"
#include "type_define/type_define.h"
#include "thread_local/storage.h"
#include "constants/tips_id_constants.h"
#include "game_logic/scene/util/player_change_scene_util.h"
#include "scene/util/scene_util.h"

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
	PlayerChangeSceneUtil::InitChangeSceneQueue(playerEntity);
	return playerEntity;
}

CentreChangeSceneInfo& GetPlayerFrontChangeSceneInfo(entt::entity playerEntity)
{
	// Get the front change scene info for the player
	return tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.front();
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
			globalSceneList.push_back(SceneUtil::CreateScene2GameNode({ .node = mainSceneNode, .sceneInfo = sceneInfo }));
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
		EXPECT_EQ(kOK, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	}
	CentreChangeSceneInfo changeInfo;
	EXPECT_EQ(kEnterSceneChangingGs, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
}

// Test: Directly switch within the same game server, should succeed directly
TEST(PlayerChangeScene, ChangeSameGsSceneNotEnqueue)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });
	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eSameGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());
}

TEST(PlayerChangeScene, Gs1SceneToGs2SceneInZoneServer)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs); // todo scene logic
	changeInfo.set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
	EXPECT_EQ(kOK, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));

	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(!tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eEnterGsSceneSucceed);

	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(!tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());
}

TEST(PlayerChangeScene, DiffGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
	EXPECT_EQ(kOK, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(!tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());

	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());
}

TEST(PlayerChangeScene, SameGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eSameGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());
}

TEST(PlayerChangeScene, CrossServerDiffGs)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_FALSE(tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());
}

// Test various states
TEST(PlayerChangeScene, ServerCrush)
{
	const auto playerEntity = CreatePlayerEntity();
	const auto fromSceneEntity = *(globalSceneList.begin()++);
	const auto sceneId = tls.sceneRegistry.get<SceneInfo>(fromSceneEntity).guid();
	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });

	CentreChangeSceneInfo changeInfo;
	changeInfo.set_guid(sceneId);
	changeInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs); // todo scene logic
	EXPECT_EQ(kOK, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_FALSE(tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
	PlayerChangeSceneUtil::PopFrontChangeSceneQueue(playerEntity); // crash
	EXPECT_TRUE(tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());

	SceneUtil::EnterScene({ fromSceneEntity, playerEntity });
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eLeaveGsScene);
	EXPECT_EQ(kOK, PlayerChangeSceneUtil::PushChangeSceneInfo(playerEntity, changeInfo));
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_FALSE(tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());
	GetPlayerFrontChangeSceneInfo(playerEntity).set_change_gs_status(CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(playerEntity);
	EXPECT_TRUE(tls.registry.get<CentrePlayerChangeSceneQueueComp>(playerEntity).changeSceneQueue.empty());
}

int32_t main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
