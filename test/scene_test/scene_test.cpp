#include <gtest/gtest.h>

#include "muduo/base/Logging.h"
#include "scene/system/scene_system.h"
#include "scene/comp/scene_comp.h"
#include "thread_local/storage.h"
#include "pbc/scene_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"

#include "proto/logic/component/game_node_comp.pb.h"
#include "proto/logic/component/scene_comp.pb.h"
#include "proto/common/node.pb.h"

using GameNodePlayerInfoPtrPBComponent = std::shared_ptr<GameNodePlayerInfoPBComponent>;

const std::size_t kConfigSceneListSize = 50;
const std::size_t kPerSceneConfigSize = 2;

entt::entity CreateMainSceneNode()
{
	const auto node = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
	AddMainSceneNodeComponent(tls.GetNodeRegistry(eNodeType::SceneNodeService), node);
	return node;
}

TEST(SceneSystemTests, CreateMainScene)
{
	const SceneUtil sceneSystem;

	CreateGameNodeSceneParam createParams;
	const auto serverEntity1 = CreateMainSceneNode();

	createParams.node = serverEntity1;
	for (uint32_t i = 0; i < kConfigSceneListSize; ++i)
	{
		createParams.sceneInfo.set_scene_confid(i);
		for (uint32_t j = 0; j < kPerSceneConfigSize; ++j)
		{
			sceneSystem.CreateSceneToSceneNode(createParams);
		}
		EXPECT_EQ(sceneSystem.GetScenesSize(i), kPerSceneConfigSize);
	}
	EXPECT_EQ(sceneSystem.GetScenesSize(), kConfigSceneListSize * kPerSceneConfigSize);
}

TEST(SceneSystemTests, CreateScene2Server)
{
	SceneUtil sceneSystem;
	const auto node1 = CreateMainSceneNode();
	const auto node2 = CreateMainSceneNode();

	CreateGameNodeSceneParam createParams1;
	CreateGameNodeSceneParam createParams2;

	createParams1.sceneInfo.set_scene_confid(2);
	createParams1.node = node1;

	createParams2.sceneInfo.set_scene_confid(3);
	createParams2.node = node2;

	sceneSystem.CreateSceneToSceneNode(createParams1);
	sceneSystem.CreateSceneToSceneNode(createParams2);

	const auto nodeComp1 = tls.GetNodeRegistry(eNodeType::SceneNodeService).try_get<NodeSceneComp>(node1);
	if (nodeComp1)
	{
		EXPECT_EQ(1, nodeComp1->GetTotalSceneCount());
	}

	const auto nodeComp2 = tls.GetNodeRegistry(eNodeType::SceneNodeService).try_get<NodeSceneComp>(node2);
	if (nodeComp2)
	{
		EXPECT_EQ(1, nodeComp2->GetTotalSceneCount());
	}

	EXPECT_EQ(1, sceneSystem.GetScenesSize(createParams1.sceneInfo.scene_confid()));
	EXPECT_EQ(1, sceneSystem.GetScenesSize(createParams2.sceneInfo.scene_confid()));
	EXPECT_EQ(2, sceneSystem.GetScenesSize());
}

TEST(SceneSystemTests, DestroyScene)
{
	SceneUtil sceneSystem;
	const auto node1 = CreateMainSceneNode();

	CreateGameNodeSceneParam createParams1;
	createParams1.node = node1;
	const auto scene = sceneSystem.CreateSceneToSceneNode(createParams1);

	EXPECT_EQ(1, sceneSystem.GetScenesSize());
	EXPECT_EQ(1, sceneSystem.GetScenesSize(createParams1.sceneInfo.scene_confid()));

	auto serverComp1 = tls.GetNodeRegistry(eNodeType::SceneNodeService).try_get<NodeSceneComp>(node1);
	if (serverComp1)
	{
		EXPECT_EQ(1, serverComp1->GetTotalSceneCount());
	}

	sceneSystem.DestroyScene({ node1, scene });

	EXPECT_TRUE(sceneSystem.IsSceneEmpty());
	EXPECT_FALSE(sceneSystem.ConfigSceneListNotEmpty(createParams1.sceneInfo.scene_confid()));
	EXPECT_TRUE(sceneSystem.IsSceneEmpty());
	EXPECT_EQ(sceneSystem.GetScenesSize(), sceneSystem.GetScenesSize());
	EXPECT_FALSE(tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(scene));
}

TEST(SceneSystemTests, DestroyServer)
{
	SceneUtil sceneSystem;

	auto node1 = CreateMainSceneNode();
	auto node2 = CreateMainSceneNode();

	CreateGameNodeSceneParam createParams1;
	CreateGameNodeSceneParam createParams2;

	createParams1.sceneInfo.set_scene_confid(3);
	createParams1.node = node1;

	createParams2.sceneInfo.set_scene_confid(2);
	createParams2.node = node2;

	auto scene1 = sceneSystem.CreateSceneToSceneNode(createParams1);
	auto scene2 = sceneSystem.CreateSceneToSceneNode(createParams2);

	EXPECT_EQ(1, tls.GetNodeRegistry(eNodeType::SceneNodeService).get<NodeSceneComp>(node1).GetTotalSceneCount());
	EXPECT_EQ(1, tls.GetNodeRegistry(eNodeType::SceneNodeService).get<NodeSceneComp>(node2).GetTotalSceneCount());

	EXPECT_EQ(2, sceneSystem.GetScenesSize());
	EXPECT_EQ(sceneSystem.GetScenesSize(), sceneSystem.GetScenesSize());

	sceneSystem.HandleDestroyGameNode(node1);

	EXPECT_FALSE(tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(node1));
	EXPECT_FALSE(tls.sceneRegistry.valid(scene1));
	EXPECT_TRUE(tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(node2));
	EXPECT_TRUE(tls.sceneRegistry.valid(scene2));

	EXPECT_EQ(1, tls.GetNodeRegistry(eNodeType::SceneNodeService).get<NodeSceneComp>(node2).GetTotalSceneCount());
	EXPECT_EQ(1, sceneSystem.GetScenesSize());
	EXPECT_EQ(0, sceneSystem.GetScenesSize(createParams1.sceneInfo.scene_confid()));
	EXPECT_EQ(1, sceneSystem.GetScenesSize(createParams2.sceneInfo.scene_confid()));

	sceneSystem.HandleDestroyGameNode(node2);

	EXPECT_EQ(0, sceneSystem.GetScenesSize());
	EXPECT_FALSE(tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(node1));
	EXPECT_FALSE(tls.sceneRegistry.valid(scene1));
	EXPECT_FALSE(tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(node2));
	EXPECT_FALSE(tls.sceneRegistry.valid(scene2));

	EXPECT_EQ(0, sceneSystem.GetScenesSize(createParams1.sceneInfo.scene_confid()));
	EXPECT_EQ(0, sceneSystem.GetScenesSize(createParams2.sceneInfo.scene_confid()));
	EXPECT_EQ(sceneSystem.GetScenesSize(), sceneSystem.GetScenesSize());
}

TEST(SceneSystemTests, PlayerLeaveEnterScene)
{
	SceneUtil sceneSystem;

	auto node1 = CreateMainSceneNode();
	auto node2 = CreateMainSceneNode();

	CreateGameNodeSceneParam createParams1;
	CreateGameNodeSceneParam createParams2;

	createParams1.sceneInfo.set_scene_confid(3);
	createParams1.node = node1;

	createParams2.sceneInfo.set_scene_confid(2);
	createParams2.node = node2;

	auto scene1 = sceneSystem.CreateSceneToSceneNode(createParams1);
	auto scene2 = sceneSystem.CreateSceneToSceneNode(createParams2);

	EnterSceneParam enterParam1;
	enterParam1.scene = scene1;

	EnterSceneParam enterParam2;
	enterParam2.scene = scene2;

	uint32_t playerSize = 100;
	EntityUnorderedSet playerEntitySet1;
	EntityUnorderedSet playerEntitiesSet2;

	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto playerEntity = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();

		if (i % 2 == 0)
		{
			playerEntitySet1.emplace(playerEntity);
			enterParam1.enter = playerEntity;
			sceneSystem.EnterScene(enterParam1);
		}
		else
		{
			playerEntitiesSet2.emplace(playerEntity);
			enterParam2.enter = playerEntity;
			sceneSystem.EnterScene(enterParam2);
		}
	}

	const auto& scenesPlayers1 = tls.sceneRegistry.get<ScenePlayers>(scene1);
	const auto& scenesPlayers2 = tls.sceneRegistry.get<ScenePlayers>(scene2);

	for (const auto& playerEntity : playerEntitySet1)
	{
		EXPECT_TRUE(scenesPlayers1.find(playerEntity) != scenesPlayers1.end());
		EXPECT_TRUE(tls.registry.get<SceneEntityComp>(playerEntity).sceneEntity == scene1);
	}

	for (const auto& playerEntity : playerEntitiesSet2)
	{
		EXPECT_TRUE(scenesPlayers2.find(playerEntity) != scenesPlayers2.end());
		EXPECT_TRUE(tls.registry.get<SceneEntityComp>(playerEntity).sceneEntity == scene2);
	}

	EXPECT_EQ(tls.GetNodeRegistry(eNodeType::SceneNodeService).get<GameNodePlayerInfoPtrPBComponent>(node1)->player_size(), playerSize / 2);
	EXPECT_EQ(tls.GetNodeRegistry(eNodeType::SceneNodeService).get<GameNodePlayerInfoPtrPBComponent>(node2)->player_size(), playerSize / 2);

	LeaveSceneParam leaveParam1;
	for (const auto& playerEntity : playerEntitySet1)
	{
		leaveParam1.leaver = playerEntity;
		sceneSystem.LeaveScene(leaveParam1);
		EXPECT_FALSE(scenesPlayers1.find(playerEntity) != scenesPlayers1.end());
		EXPECT_EQ(tls.registry.try_get<SceneEntityComp>(playerEntity), nullptr);
	}

	EXPECT_EQ(tls.GetNodeRegistry(eNodeType::SceneNodeService).get<GameNodePlayerInfoPtrPBComponent>(node1)->player_size(), 0);

	LeaveSceneParam leaveParam2;
	for (const auto& playerEntity : playerEntitiesSet2)
	{
		leaveParam2.leaver = playerEntity;
		sceneSystem.LeaveScene(leaveParam2);
		EXPECT_FALSE(scenesPlayers2.find(playerEntity) != scenesPlayers2.end());
		EXPECT_EQ(tls.registry.try_get<SceneEntityComp>(playerEntity), nullptr);
	}

	EXPECT_EQ(tls.GetNodeRegistry(eNodeType::SceneNodeService).get<GameNodePlayerInfoPtrPBComponent>(node2)->player_size(), 0);

	auto& scenesPlayers11 = tls.sceneRegistry.get<ScenePlayers>(scene1);
	auto& scenesPlayers22 = tls.sceneRegistry.get<ScenePlayers>(scene2);
	EXPECT_TRUE(scenesPlayers11.empty());


	EXPECT_TRUE(scenesPlayers22.empty());

	tls.GetNodeRegistry(eNodeType::SceneNodeService).destroy(node1);
	tls.GetNodeRegistry(eNodeType::SceneNodeService).destroy(node2);
}

TEST(GS, MainTainWeightRoundRobinMainScene)
{
	tls.GetNodeRegistry(eNodeType::SceneNodeService).clear();
	SceneUtil sm;
	NodeSceneSystem nodeSystem;
	EntityUnorderedSet serverEntities;
	const uint32_t serverSize = 2;
	const uint32_t perServerScene = 2;
	EntityUnorderedSet sceneEntities;

	for (uint32_t i = 0; i < serverSize; ++i)
	{
		serverEntities.emplace(CreateMainSceneNode());
	}

	CreateGameNodeSceneParam createServerSceneParam;
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.sceneInfo.set_scene_confid(i);
		for (auto& it : serverEntities)
		{
			createServerSceneParam.node = it;
			auto scene = sm.CreateSceneToSceneNode(createServerSceneParam);
			if (sceneEntities.empty())
			{
				sceneEntities.emplace(scene);
			}
		}
	}

	constexpr uint32_t playerSize = 1000;

	std::unordered_map<entt::entity, entt::entity> playerScene1;

	EnterSceneParam enterParam1;
	// TODO: Enter the first scene
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		for (auto&& sceneEntity : sceneEntities)
		{
			enterParam1.enter = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
			enterParam1.scene = sceneEntity;
			playerScene1.emplace(enterParam1.enter, enterParam1.scene);
			sm.EnterScene(enterParam1);
		}
	}
	NodeSceneSystem::SetNodeState(*serverEntities.begin(), NodeState::kMaintain);

	GetSceneParams weightRoundRobinScene;
	weightRoundRobinScene.sceneConfigurationId = 0;
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto canEnter = nodeSystem.FindSceneWithMinPlayerCount(weightRoundRobinScene);
		EXPECT_TRUE(canEnter != entt::null);
	}

	weightRoundRobinScene.sceneConfigurationId = 1;
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto canEnter = nodeSystem.FindSceneWithMinPlayerCount(weightRoundRobinScene);
		EXPECT_TRUE(canEnter != entt::null);
	}
}


TEST(GS, CompelToChangeScene)
{
	SceneUtil sm;

	auto node1 = CreateMainSceneNode();
	auto node2 = CreateMainSceneNode();

	CreateGameNodeSceneParam server1Param;
	CreateGameNodeSceneParam server2Param;

	server1Param.sceneInfo.set_scene_confid(2);
	server1Param.node = node1;

	server2Param.sceneInfo.set_scene_confid(2);
	server2Param.node = node2;

	const auto scene1 = sm.CreateSceneToSceneNode(server1Param);
	const auto scene2 = sm.CreateSceneToSceneNode(server2Param);

	EnterSceneParam enterParam1;
	enterParam1.scene = scene1;

	EnterSceneParam enterParam2;
	enterParam2.scene = scene2;

	constexpr uint32_t playerSize = 100;
	EntityUnorderedSet playerList1;
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto player = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
		playerList1.emplace(player);
		enterParam1.enter = player;
		sm.EnterScene(enterParam1);
	}

	CompelChangeSceneParam compelChangeParam1;
	compelChangeParam1.destNode = node2;
	compelChangeParam1.sceneConfId = server2Param.sceneInfo.scene_confid();
	for (auto& it : playerList1)
	{
		compelChangeParam1.player = it;
		sm.CompelPlayerChangeScene(compelChangeParam1);
		EXPECT_TRUE(tls.registry.try_get<SceneEntityComp>(it)->sceneEntity == scene2);
	}
	EXPECT_EQ(tls.GetNodeRegistry(eNodeType::SceneNodeService).get<GameNodePlayerInfoPtrPBComponent>(node1)->player_size(), 0);
	EXPECT_EQ(tls.GetNodeRegistry(eNodeType::SceneNodeService).get<GameNodePlayerInfoPtrPBComponent>(node2)->player_size(), playerList1.size());
	auto& scenesPlayers11 = tls.sceneRegistry.get<ScenePlayers>(scene1);
	auto& scenesPlayers22 = tls.sceneRegistry.get<ScenePlayers>(scene2);
	EXPECT_TRUE(scenesPlayers11.empty());
	EXPECT_EQ(scenesPlayers22.size(), playerList1.size());
}


TEST(GS, CrashWeightRoundRobinMainScene)
{
	SceneUtil sm;
	NodeSceneSystem nsSys;
	EntityUnorderedSet serverEntities;
	uint32_t serverSize = 2;
	uint32_t perServerScene = 2;

	EntityUnorderedSet sceneEntities;

	for (uint32_t i = 0; i < serverSize; ++i)
	{
		serverEntities.emplace(CreateMainSceneNode());
	}

	CreateGameNodeSceneParam createServerSceneParam;
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.sceneInfo.set_scene_confid(i);
		for (auto& it : serverEntities)
		{
			createServerSceneParam.node = it;
			auto e = sm.CreateSceneToSceneNode(createServerSceneParam);
			if (sceneEntities.empty())
			{
				sceneEntities.emplace(e);
			}
		}
	}

	uint32_t playerSize = 1000;

	std::unordered_map<entt::entity, entt::entity> playerScene1;

	EnterSceneParam enterParam1;

	for (uint32_t i = 0; i < playerSize; ++i)
	{
		for (auto it : sceneEntities)
		{
			auto pE = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
			enterParam1.enter = pE;
			enterParam1.scene = it;
			playerScene1.emplace(enterParam1.enter, enterParam1.scene);
			sm.EnterScene(enterParam1);
		}
	}

	nsSys.SetNodeState(*serverEntities.begin(), NodeState::kCrash);

	uint32_t sceneConfigId0 = 0;
	GetSceneParams weightRoundRobinScene;
	weightRoundRobinScene.sceneConfigurationId = sceneConfigId0;
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto canEnter = nsSys.FindSceneWithMinPlayerCount(weightRoundRobinScene);
		EXPECT_TRUE(canEnter != entt::null);
	}
}


//崩溃时候的消息不能处理
TEST(GS, CrashMovePlayer2NewServer)
{
	SceneUtil sm;
	NodeSceneSystem nsSys;
	EntityUnorderedSet nodeList;
	EntityUnorderedSet sceneList;
	uint32_t nodeSize = 2;
	uint32_t perNodeScene = 2;
	entt::entity firstScene = entt::null;

	for (uint32_t i = 0; i < nodeSize; ++i)
	{
		nodeList.emplace(CreateMainSceneNode());
	}

	CreateGameNodeSceneParam createNodeSceneParam;
	for (uint32_t i = 0; i < perNodeScene; ++i)
	{
		createNodeSceneParam.sceneInfo.set_scene_confid(i);
		for (auto& it : nodeList)
		{
			createNodeSceneParam.node = it;
			auto e = sm.CreateSceneToSceneNode(createNodeSceneParam);
			sceneList.emplace(e);
			if (firstScene == entt::null)
			{
				firstScene = e;
			}
		}
	}

	uint32_t playerSize = 1000;

	std::unordered_map<entt::entity, entt::entity> playerScene1;

	EnterSceneParam enterParam1;

	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto player = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
		enterParam1.enter = player;
		enterParam1.scene = firstScene;
		playerScene1.emplace(enterParam1.enter, enterParam1.scene);
		sm.EnterScene(enterParam1);
	}

	nsSys.SetNodeState(*nodeList.begin(), NodeState::kCrash);

	entt::entity crashNode = *nodeList.begin();
	entt::entity replaceNode = *(++nodeList.begin());
	sm.ReplaceCrashGameNode(crashNode, replaceNode);

	EXPECT_FALSE(tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(crashNode));
	nodeList.erase(crashNode);
	for (auto& it : nodeList)
	{
		auto& serverScene = tls.GetNodeRegistry(eNodeType::SceneNodeService).get<NodeSceneComp>(it);
		EXPECT_EQ(serverScene.GetTotalSceneCount(), sceneList.size());
	}
}


TEST(GS, WeightRoundRobinMainScene)
{
	tls.GetNodeRegistry(eNodeType::SceneNodeService).clear();
	SceneUtil sm;
	NodeSceneSystem nssys;
	EntityUnorderedSet node_list;
	uint32_t server_size = 10;
	uint32_t per_server_scene = 10;
	for (uint32_t i = 0; i < server_size; ++i)
	{

		node_list.emplace(CreateMainSceneNode());
	}

	CreateGameNodeSceneParam create_server_scene_param;

	for (uint32_t i = 0; i < per_server_scene; ++i)
	{
		create_server_scene_param.sceneInfo.set_scene_confid(i);
		for (auto& it : node_list)
		{
			create_server_scene_param.node = it;
			sm.CreateSceneToSceneNode(create_server_scene_param);
		}
	}

	auto enter_leave_lambda = [&node_list, server_size, per_server_scene, &sm, &nssys]()->void
		{
			uint32_t scene_config_id0 = 0;
			uint32_t scene_config_id1 = 1;
			GetSceneParams weight_round_robin_scene;
			weight_round_robin_scene.sceneConfigurationId = scene_config_id0;

			uint32_t player_size = 1000;

			std::unordered_map<entt::entity, entt::entity> player_scene1;
			EnterSceneParam enter_param1;

			EntityUnorderedSet scene_sets;

			for (uint32_t i = 0; i < player_size; ++i)
			{
				auto can_enter = nssys.FindSceneWithMinPlayerCount(weight_round_robin_scene);
				auto p_e = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
				enter_param1.enter = p_e;
				enter_param1.scene = can_enter;
				player_scene1.emplace(enter_param1.enter, can_enter);
				scene_sets.emplace(can_enter);
				sm.EnterScene(enter_param1);
			}

			uint32_t player_scene_guid = 0;
			for (auto& it : player_scene1)
			{
				auto& pse = tls.registry.get<SceneEntityComp>(it.first);
				EXPECT_TRUE(pse.sceneEntity == it.second);
				EXPECT_EQ(tls.sceneRegistry.get<SceneInfoPBComponent>(pse.sceneEntity).scene_confid(), scene_config_id0);
			}

			std::unordered_map<entt::entity, entt::entity> player_scene2;
			weight_round_robin_scene.sceneConfigurationId = scene_config_id1;
			for (uint32_t i = 0; i < player_size; ++i)
			{
				auto can_enter = nssys.FindSceneWithMinPlayerCount(weight_round_robin_scene);
				auto player = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
				enter_param1.enter = player;
				enter_param1.scene = can_enter;
				player_scene2.emplace(enter_param1.enter, enter_param1.scene);
				scene_sets.emplace(can_enter);
				sm.EnterScene(enter_param1);
			}
			player_scene_guid = 0;
			for (auto& it : player_scene2)
			{
				auto& pse = tls.registry.get<SceneEntityComp>(it.first);
				EXPECT_TRUE(pse.sceneEntity == it.second);
				EXPECT_EQ(tls.sceneRegistry.get<SceneInfoPBComponent>(pse.sceneEntity).scene_confid(), scene_config_id1);
			}

			std::size_t server_player_size = player_size * 2 / server_size;


			for (auto& it : node_list)
			{
				auto& ps = tls.GetNodeRegistry(eNodeType::SceneNodeService).get<GameNodePlayerInfoPtrPBComponent>(it);
				EXPECT_EQ((*ps).player_size(), server_player_size);
			}
			EXPECT_EQ(scene_sets.size(), std::size_t(2 * per_server_scene));

			LeaveSceneParam leave_scene;
			for (auto& it : player_scene1)
			{
				auto& pse = tls.registry.get<SceneEntityComp>(it.first);
				leave_scene.leaver = it.first;
				sm.LeaveScene(leave_scene);
			}
			for (auto& it : player_scene2)
			{
				auto& pse = tls.registry.get<SceneEntityComp>(it.first);
				leave_scene.leaver = it.first;
				sm.LeaveScene(leave_scene);
			}
			for (auto& it : node_list)
			{
				auto& ps = tls.GetNodeRegistry(eNodeType::SceneNodeService).get<GameNodePlayerInfoPtrPBComponent>(it);
				EXPECT_EQ((*ps).player_size(), 0);
			}
			for (auto& it : player_scene1)
			{
				EXPECT_EQ(tls.sceneRegistry.get<ScenePlayers>(it.second).size(), 0);
			}
			for (auto& it : player_scene2)
			{
				EXPECT_EQ(tls.sceneRegistry.get<ScenePlayers>(it.second).size(), 0);
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
	tls.GetNodeRegistry(eNodeType::SceneNodeService).clear();
	SceneUtil sm;
	NodeSceneSystem nsSys;
	EntityUnorderedSet serverEntities;
	uint32_t serverSize = 2;
	uint32_t perServerScene = 10;

	// Create server nodes
	for (uint32_t i = 0; i < serverSize; ++i)
	{
		serverEntities.emplace(CreateMainSceneNode());
	}

	// Create scenes on each server node
	CreateGameNodeSceneParam createServerSceneParam;
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.sceneInfo.set_scene_confid(i);
		for (auto& it : serverEntities)
		{
			createServerSceneParam.node = it;
			sm.CreateSceneToSceneNode(createServerSceneParam);
		}
	}

	// Set pressure on the first server node
	nsSys.SetNodePressure(*serverEntities.begin());

	uint32_t sceneConfigId0 = 0;
	uint32_t sceneConfigId1 = 1;

	GetSceneParams weightRoundRobinScene;
	weightRoundRobinScene.sceneConfigurationId = sceneConfigId0;

	std::unordered_map<entt::entity, entt::entity> playerScene1;
	EnterSceneParam enterParam1;

	// Enter players into scenes with sceneConfigId0
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		auto canEnter = nsSys.FindSceneWithMinPlayerCount(weightRoundRobinScene);
		auto playerEntity = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
		enterParam1.enter = playerEntity;
		enterParam1.scene = canEnter;
		playerScene1.emplace(enterParam1.enter, enterParam1.scene);
		sm.EnterScene(enterParam1);
	}

	// Clear pressure on the first server node
	nsSys.ClearNodePressure(*serverEntities.begin());

	std::unordered_map<entt::entity, entt::entity> playerScene2;
	weightRoundRobinScene.sceneConfigurationId = sceneConfigId1;

	// Enter players into scenes with sceneConfigId1
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		auto canEnter = nsSys.FindSceneWithMinPlayerCount(weightRoundRobinScene);
		auto playerEntity = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
		enterParam1.enter = playerEntity;
		enterParam1.scene = canEnter;
		playerScene2.emplace(enterParam1.enter, enterParam1.scene);
		sm.EnterScene(enterParam1);
	}
}

TEST(GS, EnterDefaultScene)
{
	const auto gameNode = CreateMainSceneNode();
	CreateGameNodeSceneParam createGSSceneParam{ gameNode };

	// Create multiple scenes for the game node
	for (uint32_t i = 1; i < kConfigSceneListSize; ++i)
	{
		createGSSceneParam.sceneInfo.set_scene_confid(i);
		for (uint32_t j = 0; j < kPerSceneConfigSize; ++j)
		{
			SceneUtil::CreateSceneToSceneNode(createGSSceneParam);
		}
	}

	// Create a player entity
	const auto player = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();

	// Enter the default scene with the player
	const EnterDefaultSceneParam enterParam{ player };
	SceneUtil::EnterDefaultScene(enterParam);

	// Verify the player is in the default scene
	const auto [sceneEntity] = tls.registry.get<SceneEntityComp>(player);
	const auto& sceneInfo = tls.sceneRegistry.get<SceneInfoPBComponent>(sceneEntity);
	EXPECT_EQ(sceneInfo.scene_confid(), kDefaultSceneId);
}


struct TestNodeId
{
    uint32_t node_id_{ 0 };
};

TEST(GS, GetNotFullMainSceneWhenSceneFull)
{
	tls.GetNodeRegistry(eNodeType::SceneNodeService).clear();
	SceneUtil sm;
	NodeSceneSystem nssys;
	EntityUnorderedSet serverEntities;
	uint32_t serverSize = 10;
	uint32_t perServerScene = 10;

	// Create server entities and assign node IDs
	for (uint32_t i = 0; i < serverSize; ++i)
	{
		auto server = CreateMainSceneNode();
		serverEntities.emplace(server);
		tls.GetNodeRegistry(eNodeType::SceneNodeService).emplace<TestNodeId>(server).node_id_ = i;
	}

	CreateGameNodeSceneParam createServerSceneParam;

	// Create scenes for each server entity
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.sceneInfo.set_scene_confid(i);
		for (auto& it : serverEntities)
		{
			createServerSceneParam.node = it;
			auto scene1 = sm.CreateSceneToSceneNode(createServerSceneParam);
			tls.GetNodeRegistry(eNodeType::SceneNodeService).emplace<TestNodeId>(scene1, tls.GetNodeRegistry(eNodeType::SceneNodeService).get<TestNodeId>(it));
			auto scene2 = sm.CreateSceneToSceneNode(createServerSceneParam);
			tls.GetNodeRegistry(eNodeType::SceneNodeService).emplace<TestNodeId>(scene2, tls.GetNodeRegistry(eNodeType::SceneNodeService).get<TestNodeId>(it));
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
			EnterSceneParam enterParam1;
			EntityUnorderedSet sceneSets;

			// Enter players into scenes with sceneConfigId0
			for (uint32_t i = 0; i < playerSize; ++i)
			{
				auto canEnter = nssys.FindNotFullScene(weightRoundRobinScene);
				if (canEnter == entt::null)
				{
					continue;
				}
				auto playerEntity = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
				enterParam1.enter = playerEntity;
				enterParam1.scene = canEnter;
				playerScene1.emplace(enterParam1.enter, canEnter);
				sceneSets.emplace(canEnter);
				sm.EnterScene(enterParam1);
			}

			// Verify players are correctly placed in scenes and sceneConfigId0 is assigned
			for (auto& it : playerScene1)
			{
				auto& pse = tls.registry.get<SceneEntityComp>(it.first);
				EXPECT_TRUE(pse.sceneEntity == it.second);
				EXPECT_EQ(tls.sceneRegistry.get<SceneInfoPBComponent>(pse.sceneEntity).scene_confid(), sceneConfigId0);
			}

			// Enter players into scenes with sceneConfigId1
			std::unordered_map<entt::entity, entt::entity> playerScene2;
			weightRoundRobinScene.sceneConfigurationId = sceneConfigId1;
			for (uint32_t i = 0; i < playerSize; ++i)
			{
				auto canEnter = nssys.FindNotFullScene(weightRoundRobinScene);
				if (canEnter == entt::null)
				{
					continue;
				}
				auto playerEntity = tls.GetNodeRegistry(eNodeType::SceneNodeService).create();
				enterParam1.enter = playerEntity;
				enterParam1.scene = canEnter;
				playerScene2.emplace(enterParam1.enter, enterParam1.scene);
				sceneSets.emplace(canEnter);
				sm.EnterScene(enterParam1);
			}

			// Verify players are correctly placed in scenes and sceneConfigId1 is assigned
			for (auto& it : playerScene2)
			{
				auto& pse = tls.registry.get<SceneEntityComp>(it.first);
				EXPECT_TRUE(pse.sceneEntity == it.second);
				EXPECT_EQ(tls.sceneRegistry.get<SceneInfoPBComponent>(pse.sceneEntity).scene_confid(), sceneConfigId1);
			}

			// Calculate expected player distribution across servers
			std::size_t serverPlayerSize = playerSize * 2 / serverSize;
			std::size_t remainServerSize = playerSize * 2 - kMaxScenePlayerSize * 2;

			// Verify player distribution across server entities
			for (auto& it : serverEntities)
			{
				auto& ps = tls.GetNodeRegistry(eNodeType::SceneNodeService).get<GameNodePlayerInfoPtrPBComponent>(it);
				if (tls.GetNodeRegistry(eNodeType::SceneNodeService).get<TestNodeId>(it).node_id_ == 9)
				{
					EXPECT_EQ((*ps).player_size(), kMaxServerPlayerSize);
				}
				else if (tls.GetNodeRegistry(eNodeType::SceneNodeService).get<TestNodeId>(it).node_id_ == 8)
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
			LeaveSceneParam leaveScene;
			for (auto& it : playerScene1)
			{
				leaveScene.leaver = it.first;
				sm.LeaveScene(leaveScene);
			}

			// Leave scenes for playerScene2
			for (auto& it : playerScene2)
			{
				leaveScene.leaver = it.first;
				sm.LeaveScene(leaveScene);
			}

			// Verify all server entities have no players after leaving scenes
			for (auto& it : serverEntities)
			{
				auto& ps = tls.GetNodeRegistry(eNodeType::SceneNodeService).get<GameNodePlayerInfoPtrPBComponent>(it);
				EXPECT_EQ((*ps).player_size(), 0);
			}

			// Verify no scenes have players after leaving scenes
			for (auto& it : playerScene1)
			{
				EXPECT_EQ(tls.sceneRegistry.get<ScenePlayers>(it.second).size(), 0);
			}
			for (auto& it : playerScene2)
			{
				EXPECT_EQ(tls.sceneRegistry.get<ScenePlayers>(it.second).size(), 0);
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
    tls.GetNodeRegistry(eNodeType::SceneNodeService).clear();
}

TEST(GS, CheckEnterRoomScene)
{
	// Create a scene with creators set
	SceneInfoPBComponent sceneInfo;
	for (uint64_t i = 1; i < 10; ++i)
	{
		sceneInfo.mutable_creators()->emplace(i, false); // Assuming creators are added with a boolean indicating creator status
	}
	auto scene = SceneUtil::CreateSceneToSceneNode({ .node = CreateMainSceneNode(), .sceneInfo = sceneInfo });

	// Create players with different GUIDs
	const auto player1 = tls.registry.create();
	tls.registry.emplace<Guid>(player1, 1);   // Player 1 with GUID 1
	const auto player2 = tls.registry.create();
	tls.registry.emplace<Guid>(player2, 100); // Player 2 with GUID 100

	// Test cases
	EXPECT_EQ(kSuccess, SceneUtil::CheckPlayerEnterScene({ .scene = scene, .enter = player1 }));
	EXPECT_EQ(kCheckEnterSceneCreator, SceneUtil::CheckPlayerEnterScene({ .scene = scene, .enter = player2 }));
}


int32_t main(int argc, char** argv)
{
	muduo::Logger::setLogLevel(muduo::Logger::ERROR_);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
