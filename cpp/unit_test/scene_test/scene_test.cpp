#include <gtest/gtest.h>

#include "scene/system/room_system.h"
#include "modules/scene/system/room_common.h"
#include "modules/scene/comp/scene_comp.h"
#include "table/proto/tip/scene_error_tip.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"

#include "proto/logic/component/game_node_comp.pb.h"
#include "proto/common/node.pb.h"
#include "threading/node_context_manager.h"
#include <threading/registry_manager.h>
#include <muduo/base/Logging.h>
#include <modules/scene/comp/room_node_comp.h>
#include <scene/system/room_node_state.h>
#include <scene/system/room_selector.h>

const std::size_t kConfigSceneListSize = 50;
const std::size_t kPerSceneConfigSize = 2;

entt::entity CreateMainSceneNode()
{
	const auto node = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).create();
	AddMainRoomToNodeComponent(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService), node);
	return node;
}

TEST(SceneSystemTests, CreateMainScene)
{
	const RoomUtil sceneSystem;

	CreateRoomOnNodeRoomParam createParams;
	const auto serverEntity1 = CreateMainSceneNode();

	createParams.node = serverEntity1;
	for (uint32_t i = 0; i < kConfigSceneListSize; ++i)
	{
		createParams.roomInfo.set_scene_confid(i);
		for (uint32_t j = 0; j < kPerSceneConfigSize; ++j)
		{
			RoomCommon::CreateRoomOnRoomNode(createParams);
		}
		EXPECT_EQ(RoomCommon::GetRoomsSize(i), kPerSceneConfigSize);
	}
	EXPECT_EQ(RoomCommon::GetRoomsSize(), kConfigSceneListSize * kPerSceneConfigSize);
}

TEST(SceneSystemTests, CreateScene2Server)
{
	RoomUtil sceneSystem;
	const auto node1 = CreateMainSceneNode();
	const auto node2 = CreateMainSceneNode();

	CreateRoomOnNodeRoomParam createParams1;
	CreateRoomOnNodeRoomParam createParams2;

	createParams1.roomInfo.set_scene_confid(2);
	createParams1.node = node1;

	createParams2.roomInfo.set_scene_confid(3);
	createParams2.node = node2;

	RoomCommon::CreateRoomOnRoomNode(createParams1);
	RoomCommon::CreateRoomOnRoomNode(createParams2);

	const auto nodeComp1 = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<NodeRoomComp>(node1);
	if (nodeComp1)
	{
		EXPECT_EQ(1, nodeComp1->GetTotalRoomCount());
	}

	const auto nodeComp2 = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<NodeRoomComp>(node2);
	if (nodeComp2)
	{
		EXPECT_EQ(1, nodeComp2->GetTotalRoomCount());
	}

	EXPECT_EQ(1, RoomCommon::GetRoomsSize(createParams1.roomInfo.scene_confid()));
	EXPECT_EQ(1, RoomCommon::GetRoomsSize(createParams2.roomInfo.scene_confid()));
	EXPECT_EQ(2, RoomCommon::GetRoomsSize());
}

TEST(SceneSystemTests, DestroyScene)
{
	RoomUtil sceneSystem;
	const auto node1 = CreateMainSceneNode();

	CreateRoomOnNodeRoomParam createParams1;
	createParams1.node = node1;
	const auto scene = RoomCommon::CreateRoomOnRoomNode(createParams1);

	EXPECT_EQ(1, RoomCommon::GetRoomsSize());
	EXPECT_EQ(1, RoomCommon::GetRoomsSize(createParams1.roomInfo.scene_confid()));

	auto serverComp1 = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<NodeRoomComp>(node1);
	if (serverComp1)
	{
		EXPECT_EQ(1, serverComp1->GetTotalRoomCount());
	}

	RoomCommon::DestroyRoom({ node1, scene });

	EXPECT_TRUE(RoomCommon::IsRoomEmpty());
	EXPECT_FALSE(RoomCommon::ConfigRoomListNotEmpty(createParams1.roomInfo.scene_confid()));
	EXPECT_TRUE(RoomCommon::IsRoomEmpty());
	EXPECT_EQ(RoomCommon::GetRoomsSize(), RoomCommon::GetRoomsSize());
	EXPECT_FALSE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(scene));
}

TEST(SceneSystemTests, DestroyServer)
{
	RoomUtil sceneSystem;

	auto node1 = CreateMainSceneNode();
	auto node2 = CreateMainSceneNode();

	CreateRoomOnNodeRoomParam createParams1;
	CreateRoomOnNodeRoomParam createParams2;

	createParams1.roomInfo.set_scene_confid(3);
	createParams1.node = node1;

	createParams2.roomInfo.set_scene_confid(2);
	createParams2.node = node2;

	auto scene1 = RoomCommon::CreateRoomOnRoomNode(createParams1);
	auto scene2 = RoomCommon::CreateRoomOnRoomNode(createParams2);

	EXPECT_EQ(1, tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<NodeRoomComp>(node1).GetTotalRoomCount());
	EXPECT_EQ(1, tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<NodeRoomComp>(node2).GetTotalRoomCount());

	EXPECT_EQ(2, RoomCommon::GetRoomsSize());
	EXPECT_EQ(RoomCommon::GetRoomsSize(), RoomCommon::GetRoomsSize());

	sceneSystem.HandleDestroyRoomNode(node1);

	EXPECT_FALSE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(node1));
	EXPECT_FALSE(tlsRegistryManager.roomRegistry.valid(scene1));
	EXPECT_TRUE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(node2));
	EXPECT_TRUE(tlsRegistryManager.roomRegistry.valid(scene2));

	EXPECT_EQ(1, tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<NodeRoomComp>(node2).GetTotalRoomCount());
	EXPECT_EQ(1, RoomCommon::GetRoomsSize());
	EXPECT_EQ(0, RoomCommon::GetRoomsSize(createParams1.roomInfo.scene_confid()));
	EXPECT_EQ(1, RoomCommon::GetRoomsSize(createParams2.roomInfo.scene_confid()));

	sceneSystem.HandleDestroyRoomNode(node2);

	EXPECT_EQ(0, RoomCommon::GetRoomsSize());
	EXPECT_FALSE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(node1));
	EXPECT_FALSE(tlsRegistryManager.roomRegistry.valid(scene1));
	EXPECT_FALSE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(node2));
	EXPECT_FALSE(tlsRegistryManager.roomRegistry.valid(scene2));

	EXPECT_EQ(0, RoomCommon::GetRoomsSize(createParams1.roomInfo.scene_confid()));
	EXPECT_EQ(0, RoomCommon::GetRoomsSize(createParams2.roomInfo.scene_confid()));
	EXPECT_EQ(RoomCommon::GetRoomsSize(), RoomCommon::GetRoomsSize());
}

TEST(SceneSystemTests, PlayerLeaveEnterScene)
{
	RoomUtil sceneSystem;

	auto node1 = CreateMainSceneNode();
	auto node2 = CreateMainSceneNode();

	CreateRoomOnNodeRoomParam createParams1;
	CreateRoomOnNodeRoomParam createParams2;

	createParams1.roomInfo.set_scene_confid(3);
	createParams1.node = node1;

	createParams2.roomInfo.set_scene_confid(2);
	createParams2.node = node2;

	auto scene1 = RoomCommon::CreateRoomOnRoomNode(createParams1);
	auto scene2 = RoomCommon::CreateRoomOnRoomNode(createParams2);

	EnterRoomParam enterParam1;
	enterParam1.room = scene1;

	EnterRoomParam enterParam2;
	enterParam2.room = scene2;

	uint32_t playerSize = 100;
	EntityUnorderedSet playerEntitySet1;
	EntityUnorderedSet playerEntitiesSet2;

	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto playerEntity = tlsRegistryManager.actorRegistry.create();

		if (i % 2 == 0)
		{
			playerEntitySet1.emplace(playerEntity);
			enterParam1.enter = playerEntity;
			RoomCommon::EnterRoom(enterParam1);
		}
		else
		{
			playerEntitiesSet2.emplace(playerEntity);
			enterParam2.enter = playerEntity;
			RoomCommon::EnterRoom(enterParam2);
		}
	}

	const auto& scenesPlayers1 = tlsRegistryManager.roomRegistry.get<RoomPlayers>(scene1);
	const auto& scenesPlayers2 = tlsRegistryManager.roomRegistry.get<RoomPlayers>(scene2);

	for (const auto& playerEntity : playerEntitySet1)
	{
		EXPECT_TRUE(scenesPlayers1.find(playerEntity) != scenesPlayers1.end());
		EXPECT_TRUE(tlsRegistryManager.actorRegistry.get<RoomEntityComp>(playerEntity).roomEntity == scene1);
	}

	for (const auto& playerEntity : playerEntitiesSet2)
	{
		EXPECT_TRUE(scenesPlayers2.find(playerEntity) != scenesPlayers2.end());
		EXPECT_TRUE(tlsRegistryManager.actorRegistry.get<RoomEntityComp>(playerEntity).roomEntity == scene2);
	}

	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<RoomNodePlayerStatsPtrPbComponent>(node1)->player_size(), playerSize / 2);
	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<RoomNodePlayerStatsPtrPbComponent>(node2)->player_size(), playerSize / 2);

	LeaveRoomParam leaveParam1;
	for (const auto& playerEntity : playerEntitySet1)
	{
		leaveParam1.leaver = playerEntity;
		RoomCommon::LeaveRoom(leaveParam1);
		EXPECT_FALSE(scenesPlayers1.find(playerEntity) != scenesPlayers1.end());
		EXPECT_EQ(tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(playerEntity), nullptr);
	}

	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<RoomNodePlayerStatsPtrPbComponent>(node1)->player_size(), 0);

	LeaveRoomParam leaveParam2;
	for (const auto& playerEntity : playerEntitiesSet2)
	{
		leaveParam2.leaver = playerEntity;
		RoomCommon::LeaveRoom(leaveParam2);
		EXPECT_FALSE(scenesPlayers2.find(playerEntity) != scenesPlayers2.end());
		EXPECT_EQ(tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(playerEntity), nullptr);
	}

	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<RoomNodePlayerStatsPtrPbComponent>(node2)->player_size(), 0);

	auto& scenesPlayers11 = tlsRegistryManager.roomRegistry.get<RoomPlayers>(scene1);
	auto& scenesPlayers22 = tlsRegistryManager.roomRegistry.get<RoomPlayers>(scene2);
	EXPECT_TRUE(scenesPlayers11.empty());


	EXPECT_TRUE(scenesPlayers22.empty());

	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).destroy(node1);
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).destroy(node2);
}

TEST(GS, MainTainWeightRoundRobinMainScene)
{
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
	RoomUtil sm;
	RoomNodeSelector nodeSystem;
	EntityUnorderedSet serverEntities;
	const uint32_t serverSize = 2;
	const uint32_t perServerScene = 2;
	EntityUnorderedSet sceneEntities;

	for (uint32_t i = 0; i < serverSize; ++i)
	{
		serverEntities.emplace(CreateMainSceneNode());
	}

	CreateRoomOnNodeRoomParam createServerSceneParam;
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.roomInfo.set_scene_confid(i);
		for (auto& it : serverEntities)
		{
			createServerSceneParam.node = it;
			auto scene = RoomCommon::CreateRoomOnRoomNode(createServerSceneParam);
			if (sceneEntities.empty())
			{
				sceneEntities.emplace(scene);
			}
		}
	}

	constexpr uint32_t playerSize = 1000;

	std::unordered_map<entt::entity, entt::entity> playerScene1;

	EnterRoomParam enterParam1;
	// TODO: Enter the first scene
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		for (auto&& sceneEntity : sceneEntities)
		{
			enterParam1.enter = tlsRegistryManager.actorRegistry.create();
			enterParam1.room = sceneEntity;
			playerScene1.emplace(enterParam1.enter, enterParam1.room);
			RoomCommon::EnterRoom(enterParam1);
		}
	}
	RoomNodeStateSystem::MakeNodeState(*serverEntities.begin(), NodeState::kMaintain);

	GetSceneParams weightRoundRobinScene;
	weightRoundRobinScene.sceneConfigurationId = 0;
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto canEnter = nodeSystem.SelectLeastLoadedScene(weightRoundRobinScene);
		EXPECT_TRUE(canEnter != entt::null);
	}

	weightRoundRobinScene.sceneConfigurationId = 1;
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto canEnter = nodeSystem.SelectLeastLoadedScene(weightRoundRobinScene);
		EXPECT_TRUE(canEnter != entt::null);
	}
}


TEST(GS, CompelToChangeScene)
{
	RoomUtil sm;

	auto node1 = CreateMainSceneNode();
	auto node2 = CreateMainSceneNode();

	CreateRoomOnNodeRoomParam server1Param;
	CreateRoomOnNodeRoomParam server2Param;

	server1Param.roomInfo.set_scene_confid(2);
	server1Param.node = node1;

	server2Param.roomInfo.set_scene_confid(2);
	server2Param.node = node2;

	const auto scene1 = RoomCommon::CreateRoomOnRoomNode(server1Param);
	const auto scene2 = RoomCommon::CreateRoomOnRoomNode(server2Param);

	EnterRoomParam enterParam1;
	enterParam1.room = scene1;

	EnterRoomParam enterParam2;
	enterParam2.room = scene2;

	constexpr uint32_t playerSize = 100;
	EntityUnorderedSet playerList1;
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto player = tlsRegistryManager.actorRegistry.create();
		playerList1.emplace(player);
		enterParam1.enter = player;
		RoomCommon::EnterRoom(enterParam1);
	}

	CompelChangeRoomParam compelChangeParam1;
	compelChangeParam1.destNode = node2;
	compelChangeParam1.sceneConfId = server2Param.roomInfo.scene_confid();
	for (auto& it : playerList1)
	{
		compelChangeParam1.player = it;
		sm.CompelPlayerChangeRoom(compelChangeParam1);
		EXPECT_TRUE(tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(it)->roomEntity == scene2);
	}
	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<RoomNodePlayerStatsPtrPbComponent>(node1)->player_size(), 0);
	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<RoomNodePlayerStatsPtrPbComponent>(node2)->player_size(), playerList1.size());
	auto& scenesPlayers11 = tlsRegistryManager.roomRegistry.get<RoomPlayers>(scene1);
	auto& scenesPlayers22 = tlsRegistryManager.roomRegistry.get<RoomPlayers>(scene2);
	EXPECT_TRUE(scenesPlayers11.empty());
	EXPECT_EQ(scenesPlayers22.size(), playerList1.size());
}


TEST(GS, CrashWeightRoundRobinMainScene)
{
	RoomUtil sm;
	EntityUnorderedSet serverEntities;
	uint32_t serverSize = 2;
	uint32_t perServerScene = 2;

	EntityUnorderedSet sceneEntities;

	for (uint32_t i = 0; i < serverSize; ++i)
	{
		serverEntities.emplace(CreateMainSceneNode());
	}

	CreateRoomOnNodeRoomParam createServerSceneParam;
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.roomInfo.set_scene_confid(i);
		for (auto& it : serverEntities)
		{
			createServerSceneParam.node = it;
			auto e = RoomCommon::CreateRoomOnRoomNode(createServerSceneParam);
			if (sceneEntities.empty())
			{
				sceneEntities.emplace(e);
			}
		}
	}

	uint32_t playerSize = 1000;

	std::unordered_map<entt::entity, entt::entity> playerScene1;

	EnterRoomParam enterParam1;

	for (uint32_t i = 0; i < playerSize; ++i)
	{
		for (auto it : sceneEntities)
		{
			auto pE = tlsRegistryManager.actorRegistry.create();
			enterParam1.enter = pE;
			enterParam1.room = it;
			playerScene1.emplace(enterParam1.enter, enterParam1.room);
			RoomCommon::EnterRoom(enterParam1);
		}
	}

	RoomNodeStateSystem::MakeNodeState(*serverEntities.begin(), NodeState::kCrash);

	uint32_t sceneConfigId0 = 0;
	GetSceneParams weightRoundRobinScene;
	weightRoundRobinScene.sceneConfigurationId = sceneConfigId0;
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto canEnter = RoomNodeSelector::SelectLeastLoadedScene(weightRoundRobinScene);
		EXPECT_TRUE(canEnter != entt::null);
	}
}


//崩溃时候的消息不能处理
TEST(GS, CrashMovePlayer2NewServer)
{
	RoomUtil sm;
	EntityUnorderedSet nodeList;
	EntityUnorderedSet sceneList;
	uint32_t nodeSize = 2;
	uint32_t perNodeScene = 2;
	entt::entity firstScene = entt::null;

	for (uint32_t i = 0; i < nodeSize; ++i)
	{
		nodeList.emplace(CreateMainSceneNode());
	}

	CreateRoomOnNodeRoomParam createNodeSceneParam;
	for (uint32_t i = 0; i < perNodeScene; ++i)
	{
		createNodeSceneParam.roomInfo.set_scene_confid(i);
		for (auto& it : nodeList)
		{
			createNodeSceneParam.node = it;
			auto e = RoomCommon::CreateRoomOnRoomNode(createNodeSceneParam);
			sceneList.emplace(e);
			if (firstScene == entt::null)
			{
				firstScene = e;
			}
		}
	}

	uint32_t playerSize = 1000;

	std::unordered_map<entt::entity, entt::entity> playerScene1;

	EnterRoomParam enterParam1;

	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto player = tlsRegistryManager.actorRegistry.create();
		enterParam1.enter = player;
		enterParam1.room = firstScene;
		playerScene1.emplace(enterParam1.enter, enterParam1.room);
		RoomCommon::EnterRoom(enterParam1);
	}

	RoomNodeStateSystem::MakeNodeState(*nodeList.begin(), NodeState::kCrash);

	entt::entity crashNode = *nodeList.begin();
	entt::entity replaceNode = *(++nodeList.begin());
	sm.ReplaceCrashRoomNode(crashNode, replaceNode);

	EXPECT_FALSE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(crashNode));
	nodeList.erase(crashNode);
	for (auto& it : nodeList)
	{
		auto& serverScene = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<NodeRoomComp>(it);
		EXPECT_EQ(serverScene.GetTotalRoomCount(), sceneList.size());
	}
}


TEST(GS, WeightRoundRobinMainScene)
{
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
	RoomUtil sm;
	EntityUnorderedSet node_list;
	uint32_t server_size = 10;
	uint32_t per_server_scene = 10;
	for (uint32_t i = 0; i < server_size; ++i)
	{

		node_list.emplace(CreateMainSceneNode());
	}

	CreateRoomOnNodeRoomParam create_server_scene_param;

	for (uint32_t i = 0; i < per_server_scene; ++i)
	{
		create_server_scene_param.roomInfo.set_scene_confid(i);
		for (auto& it : node_list)
		{
			create_server_scene_param.node = it;
			RoomCommon::CreateRoomOnRoomNode(create_server_scene_param);
		}
	}

	auto enter_leave_lambda = [&node_list, server_size, per_server_scene, &sm]()->void
		{
			uint32_t scene_config_id0 = 0;
			uint32_t scene_config_id1 = 1;
			GetSceneParams weight_round_robin_scene;
			weight_round_robin_scene.sceneConfigurationId = scene_config_id0;

			uint32_t player_size = 1000;

			std::unordered_map<entt::entity, entt::entity> player_scene1;
			EnterRoomParam enter_param1;

			EntityUnorderedSet scene_sets;

			for (uint32_t i = 0; i < player_size; ++i)
			{
				auto can_enter = RoomNodeSelector::SelectLeastLoadedScene(weight_round_robin_scene);
				auto p_e = tlsRegistryManager.actorRegistry.create();
				enter_param1.enter = p_e;
				enter_param1.room = can_enter;
				player_scene1.emplace(enter_param1.enter, can_enter);
				scene_sets.emplace(can_enter);
				RoomCommon::EnterRoom(enter_param1);
			}

			uint32_t player_scene_guid = 0;
			for (auto& it : player_scene1)
			{
				auto& pse = tlsRegistryManager.actorRegistry.get<RoomEntityComp>(it.first);
				EXPECT_TRUE(pse.roomEntity == it.second);
				EXPECT_EQ(tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(pse.roomEntity).scene_confid(), scene_config_id0);
			}

			std::unordered_map<entt::entity, entt::entity> player_scene2;
			weight_round_robin_scene.sceneConfigurationId = scene_config_id1;
			for (uint32_t i = 0; i < player_size; ++i)
			{
				auto can_enter = RoomNodeSelector::SelectLeastLoadedScene(weight_round_robin_scene);
				auto player = tlsRegistryManager.actorRegistry.create();
				enter_param1.enter = player;
				enter_param1.room = can_enter;
				player_scene2.emplace(enter_param1.enter, enter_param1.room);
				scene_sets.emplace(can_enter);
				RoomCommon::EnterRoom(enter_param1);
			}
			player_scene_guid = 0;
			for (auto& it : player_scene2)
			{
				auto& pse = tlsRegistryManager.actorRegistry.get<RoomEntityComp>(it.first);
				EXPECT_TRUE(pse.roomEntity == it.second);
				EXPECT_EQ(tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(pse.roomEntity).scene_confid(), scene_config_id1);
			}

			std::size_t server_player_size = player_size * 2 / server_size;


			for (auto& it : node_list)
			{
				auto& ps = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<RoomNodePlayerStatsPtrPbComponent>(it);
				EXPECT_EQ((*ps).player_size(), server_player_size);
			}
			EXPECT_EQ(scene_sets.size(), std::size_t(2 * per_server_scene));

			LeaveRoomParam leave_scene;
			for (auto& it : player_scene1)
			{
				auto& pse = tlsRegistryManager.actorRegistry.get<RoomEntityComp>(it.first);
				leave_scene.leaver = it.first;
				RoomCommon::LeaveRoom(leave_scene);
			}
			for (auto& it : player_scene2)
			{
				auto& pse = tlsRegistryManager.actorRegistry.get<RoomEntityComp>(it.first);
				leave_scene.leaver = it.first;
				RoomCommon::LeaveRoom(leave_scene);
			}
			for (auto& it : node_list)
			{
				auto& ps = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<RoomNodePlayerStatsPtrPbComponent>(it);
				EXPECT_EQ((*ps).player_size(), 0);
			}
			for (auto& it : player_scene1)
			{
				EXPECT_EQ(tlsRegistryManager.roomRegistry.get<RoomPlayers>(it.second).size(), 0);
			}
			for (auto& it : player_scene2)
			{
				EXPECT_EQ(tlsRegistryManager.roomRegistry.get<RoomPlayers>(it.second).size(), 0);
			}
		};
	for (uint32_t i = 0; i < 2; ++i)
	{
		enter_leave_lambda();
	}
	//leave 
}

TEST(GS, ServerEnterLeavePressure)
{
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
	RoomUtil sm;
	EntityUnorderedSet serverEntities;
	uint32_t serverSize = 2;
	uint32_t perServerScene = 10;

	// Create server nodes
	for (uint32_t i = 0; i < serverSize; ++i)
	{
		serverEntities.emplace(CreateMainSceneNode());
	}

	// Create scenes on each server node
	CreateRoomOnNodeRoomParam createServerSceneParam;
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.roomInfo.set_scene_confid(i);
		for (auto& it : serverEntities)
		{
			createServerSceneParam.node = it;
			RoomCommon::CreateRoomOnRoomNode(createServerSceneParam);
		}
	}

	// Set pressure on the first server node
	RoomNodeStateSystem::MakeNodePressure(*serverEntities.begin());

	uint32_t sceneConfigId0 = 0;
	uint32_t sceneConfigId1 = 1;

	GetSceneParams weightRoundRobinScene;
	weightRoundRobinScene.sceneConfigurationId = sceneConfigId0;

	std::unordered_map<entt::entity, entt::entity> playerScene1;
	EnterRoomParam enterParam1;

	// Enter players into scenes with sceneConfigId0
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		auto canEnter = RoomNodeSelector::SelectLeastLoadedScene(weightRoundRobinScene);
		auto playerEntity = tlsRegistryManager.actorRegistry.create();
		enterParam1.enter = playerEntity;
		enterParam1.room = canEnter;
		playerScene1.emplace(enterParam1.enter, enterParam1.room);
		RoomCommon::EnterRoom(enterParam1);
	}

	// Clear pressure on the first server node
	RoomNodeStateSystem::ClearNodePressure(*serverEntities.begin());

	std::unordered_map<entt::entity, entt::entity> playerScene2;
	weightRoundRobinScene.sceneConfigurationId = sceneConfigId1;

	// Enter players into scenes with sceneConfigId1
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		auto canEnter = RoomNodeSelector::SelectLeastLoadedScene(weightRoundRobinScene);
		auto playerEntity = tlsRegistryManager.actorRegistry.create();
		enterParam1.enter = playerEntity;
		enterParam1.room = canEnter;
		playerScene2.emplace(enterParam1.enter, enterParam1.room);
		RoomCommon::EnterRoom(enterParam1);
	}
}

TEST(GS, EnterDefaultScene)
{
	const auto gameNode = CreateMainSceneNode();
	CreateRoomOnNodeRoomParam createGSSceneParam{ gameNode };

	// Create multiple scenes for the game node
	for (uint32_t i = 1; i < kConfigSceneListSize; ++i)
	{
		createGSSceneParam.roomInfo.set_scene_confid(i);
		for (uint32_t j = 0; j < kPerSceneConfigSize; ++j)
		{
			RoomCommon::CreateRoomOnRoomNode(createGSSceneParam);
		}
	}

	// Create a player entity
	const auto player = tlsRegistryManager.actorRegistry.create();

	// Enter the default scene with the player
	const EnterDefaultRoomParam enterParam{ player };
	RoomUtil::EnterDefaultRoom(enterParam);

	// Verify the player is in the default scene
	const auto [sceneEntity] = tlsRegistryManager.actorRegistry.get<RoomEntityComp>(player);
	const auto& sceneInfo = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(sceneEntity);
	EXPECT_EQ(sceneInfo.scene_confid(), kDefaultSceneId);
}


struct TestNodeId
{
    uint32_t node_id_{ 0 };
};

TEST(GS, GetNotFullMainSceneWhenSceneFull)
{
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
	RoomUtil sm;
	RoomNodeSelector nssys;
	EntityUnorderedSet serverEntities;
	uint32_t serverSize = 10;
	uint32_t perServerScene = 10;

	// Create server entities and assign node IDs
	for (uint32_t i = 0; i < serverSize; ++i)
	{
		auto server = CreateMainSceneNode();
		serverEntities.emplace(server);
		tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).emplace<TestNodeId>(server).node_id_ = i;
	}

	CreateRoomOnNodeRoomParam createServerSceneParam;

	// Create scenes for each server entity
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.roomInfo.set_scene_confid(i);
		for (auto& it : serverEntities)
		{
			createServerSceneParam.node = it;
			auto scene1 = RoomCommon::CreateRoomOnRoomNode(createServerSceneParam);
			tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).emplace<TestNodeId>(scene1, tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<TestNodeId>(it));
			auto scene2 = RoomCommon::CreateRoomOnRoomNode(createServerSceneParam);
			tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).emplace<TestNodeId>(scene2, tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<TestNodeId>(it));
		}
	}

	// Lambda function to simulate entering and leaving scenes
	auto enterLeaveLambda = [&serverEntities, serverSize, perServerScene, &sm, &nssys]() -> void
		{
			uint32_t sceneConfigId0 = 0;
			uint32_t sceneConfigId1 = 1;
			GetSceneParams weightRoundRobinScene;
			weightRoundRobinScene.sceneConfigurationId = sceneConfigId0;

			uint32_t playerSize = 1001;

			std::unordered_map<entt::entity, entt::entity> playerScene1;
			EnterRoomParam enterParam1;
			EntityUnorderedSet sceneSets;

			// Enter players into scenes with sceneConfigId0
			for (uint32_t i = 0; i < playerSize; ++i)
			{
				auto canEnter = nssys.SelectAvailableRoomScene(weightRoundRobinScene);
				if (canEnter == entt::null)
				{
					continue;
				}
				auto playerEntity = tlsRegistryManager.actorRegistry.create();
				enterParam1.enter = playerEntity;
				enterParam1.room = canEnter;
				playerScene1.emplace(enterParam1.enter, canEnter);
				sceneSets.emplace(canEnter);
				RoomCommon::EnterRoom(enterParam1);
			}

			// Verify players are correctly placed in scenes and sceneConfigId0 is assigned
			for (auto& it : playerScene1)
			{
				auto& pse = tlsRegistryManager.actorRegistry.get<RoomEntityComp>(it.first);
				EXPECT_TRUE(pse.roomEntity == it.second);
				EXPECT_EQ(tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(pse.roomEntity).scene_confid(), sceneConfigId0);
			}

			// Enter players into scenes with sceneConfigId1
			std::unordered_map<entt::entity, entt::entity> playerScene2;
			weightRoundRobinScene.sceneConfigurationId = sceneConfigId1;
			for (uint32_t i = 0; i < playerSize; ++i)
			{
				auto canEnter = nssys.SelectAvailableRoomScene(weightRoundRobinScene);
				if (canEnter == entt::null)
				{
					continue;
				}
				auto playerEntity = tlsRegistryManager.actorRegistry.create();
				enterParam1.enter = playerEntity;
				enterParam1.room = canEnter;
				playerScene2.emplace(enterParam1.enter, enterParam1.room);
				sceneSets.emplace(canEnter);
				RoomCommon::EnterRoom(enterParam1);
			}

			// Verify players are correctly placed in scenes and sceneConfigId1 is assigned
			for (auto& it : playerScene2)
			{
				auto& pse = tlsRegistryManager.actorRegistry.get<RoomEntityComp>(it.first);
				EXPECT_TRUE(pse.roomEntity == it.second);
				EXPECT_EQ(tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(pse.roomEntity).scene_confid(), sceneConfigId1);
			}

			// Calculate expected player distribution across servers
			std::size_t serverPlayerSize = playerSize * 2 / serverSize;
			std::size_t remainServerSize = playerSize * 2 - kMaxScenePlayerSize * 2;

			// Verify player distribution across server entities
			for (auto& it : serverEntities)
			{
				auto& ps = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<RoomNodePlayerStatsPtrPbComponent>(it);
				if (tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<TestNodeId>(it).node_id_ == 9)
				{
					EXPECT_EQ((*ps).player_size(), kMaxServerPlayerSize);
				}
				else if (tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<TestNodeId>(it).node_id_ == 8)
				{
					EXPECT_EQ((*ps).player_size(), remainServerSize);
				}
				else
				{
					EXPECT_EQ((*ps).player_size(), 0);
				}
			}

			// Verify the total number of unique scenes entered
			EXPECT_EQ(sceneSets.size(), std::size_t(4));

			// Leave scenes for playerScene1
			LeaveRoomParam leaveScene;
			for (auto& it : playerScene1)
			{
				leaveScene.leaver = it.first;
				RoomCommon::LeaveRoom(leaveScene);
			}

			// Leave scenes for playerScene2
			for (auto& it : playerScene2)
			{
				leaveScene.leaver = it.first;
				RoomCommon::LeaveRoom(leaveScene);
			}

			// Verify all server entities have no players after leaving scenes
			for (auto& it : serverEntities)
			{
				auto& ps = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<RoomNodePlayerStatsPtrPbComponent>(it);
				EXPECT_EQ((*ps).player_size(), 0);
			}

			// Verify no scenes have players after leaving scenes
			for (auto& it : playerScene1)
			{
				EXPECT_EQ(tlsRegistryManager.roomRegistry.get<RoomPlayers>(it.second).size(), 0);
			}
			for (auto& it : playerScene2)
			{
				EXPECT_EQ(tlsRegistryManager.roomRegistry.get<RoomPlayers>(it.second).size(), 0);
			}
		};

	// Execute enterLeaveLambda twice to simulate multiple test iterations
	for (uint32_t i = 0; i < 2; ++i)
	{
		enterLeaveLambda();
	}
}


TEST(GS, CreateDungeon)
{
}

TEST(GS, Route)
{
    tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
}

TEST(GS, CheckEnterRoomScene)
{
	// Create a scene with creators set
	RoomInfoPBComponent sceneInfo;
	for (uint64_t i = 1; i < 10; ++i)
	{
		sceneInfo.mutable_creators()->emplace(i, false); // Assuming creators are added with a boolean indicating creator status
	}
	auto room = RoomCommon::CreateRoomOnRoomNode({ .node = CreateMainSceneNode(), .roomInfo = sceneInfo });

	// Create players with different GUIDs
	const auto player1 = tlsRegistryManager.actorRegistry.create();
	tlsRegistryManager.actorRegistry.emplace<Guid>(player1, 1);   // Player 1 with GUID 1
	const auto player2 = tlsRegistryManager.actorRegistry.create();
	tlsRegistryManager.actorRegistry.emplace<Guid>(player2, 100); // Player 2 with GUID 100

	// Test cases
	EXPECT_EQ(kSuccess, RoomCommon::CheckPlayerEnterRoom({ .room = room, .enter = player1 }));
	EXPECT_EQ(kCheckEnterSceneCreator, RoomCommon::CheckPlayerEnterRoom({ .room = room, .enter = player2 }));
}


int32_t main(int argc, char** argv)
{
	muduo::Logger::setLogLevel(muduo::Logger::ERROR_);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
