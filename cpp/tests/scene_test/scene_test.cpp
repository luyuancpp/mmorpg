#include <gtest/gtest.h>

#include "scene/system/scene.h"
#include "modules/scene/system/scene_common.h"
#include "modules/scene/comp/scene_comp.h"
#include "table/proto/tip/scene_error_tip.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"

#include "proto/common/component/node_player_comp.pb.h"
#include "proto/common/base/node.pb.h"
#include "thread_context/node_context_manager.h"
#include <thread_context/registry_manager.h>
#include <muduo/base/Logging.h>
#include <modules/scene/comp/scene_node_comp.h>
#include <scene/system/scene_node_state.h>
#include <scene/system/scene_node_selector.h>

const std::size_t kConfigSceneListSize = 50;
const std::size_t kPerSceneConfigSize = 2;

entt::entity CreateWorldNode()
{
	const auto node = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).create();
	AddWorldToNodeComponent(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService), node);
	return node;
}

// ---------------------------------------------------------------------------
// 场景创建 / 销毁
// ---------------------------------------------------------------------------

TEST(SceneSystemTests, CreateWorldScene)
{
	const SceneSystem sceneSystem;

	CreateSceneOnNodeSceneParam createParams;
	const auto serverEntity1 = CreateWorldNode();

	createParams.node = serverEntity1;
	for (uint32_t i = 0; i < kConfigSceneListSize; ++i)
	{
		createParams.sceneInfo.set_scene_confid(i);
		for (uint32_t j = 0; j < kPerSceneConfigSize; ++j)
		{
			SceneCommon::CreateSceneOnSceneNode(createParams);
		}
		EXPECT_EQ(SceneCommon::GetScenesSize(i), kPerSceneConfigSize);
	}
	EXPECT_EQ(SceneCommon::GetScenesSize(), kConfigSceneListSize * kPerSceneConfigSize);
}

TEST(SceneSystemTests, CreateScene2Server)
{
	SceneSystem sceneSystem;
	const auto node1 = CreateWorldNode();
	const auto node2 = CreateWorldNode();

	CreateSceneOnNodeSceneParam createParams1;
	CreateSceneOnNodeSceneParam createParams2;

	createParams1.sceneInfo.set_scene_confid(2);
	createParams1.node = node1;

	createParams2.sceneInfo.set_scene_confid(3);
	createParams2.node = node2;

	SceneCommon::CreateSceneOnSceneNode(createParams1);
	SceneCommon::CreateSceneOnSceneNode(createParams2);

	const auto nodeComp1 = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<SceneRegistryComp>(node1);
	if (nodeComp1)
	{
		EXPECT_EQ(1, nodeComp1->GetTotalSceneCount());
	}

	const auto nodeComp2 = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<SceneRegistryComp>(node2);
	if (nodeComp2)
	{
		EXPECT_EQ(1, nodeComp2->GetTotalSceneCount());
	}

	EXPECT_EQ(1, SceneCommon::GetScenesSize(createParams1.sceneInfo.scene_confid()));
	EXPECT_EQ(1, SceneCommon::GetScenesSize(createParams2.sceneInfo.scene_confid()));
	EXPECT_EQ(2, SceneCommon::GetScenesSize());
}

TEST(SceneSystemTests, DestroyScene)
{
	SceneSystem sceneSystem;
	const auto node1 = CreateWorldNode();

	CreateSceneOnNodeSceneParam createParams1;
	createParams1.node = node1;
	const auto scene = SceneCommon::CreateSceneOnSceneNode(createParams1);

	EXPECT_EQ(1, SceneCommon::GetScenesSize());
	EXPECT_EQ(1, SceneCommon::GetScenesSize(createParams1.sceneInfo.scene_confid()));

	auto serverComp1 = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<SceneRegistryComp>(node1);
	if (serverComp1)
	{
		EXPECT_EQ(1, serverComp1->GetTotalSceneCount());
	}

	SceneCommon::DestroyScene({node1, scene});

	EXPECT_TRUE(SceneCommon::IsSceneEmpty());
	EXPECT_FALSE(SceneCommon::ConfigSceneListNotEmpty(createParams1.sceneInfo.scene_confid()));
	EXPECT_TRUE(SceneCommon::IsSceneEmpty());
	EXPECT_EQ(SceneCommon::GetScenesSize(), SceneCommon::GetScenesSize());
	EXPECT_FALSE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(scene));
}

TEST(SceneSystemTests, DestroyServer)
{
	SceneSystem sceneSystem;

	auto node1 = CreateWorldNode();
	auto node2 = CreateWorldNode();

	CreateSceneOnNodeSceneParam createParams1;
	CreateSceneOnNodeSceneParam createParams2;

	createParams1.sceneInfo.set_scene_confid(3);
	createParams1.node = node1;

	createParams2.sceneInfo.set_scene_confid(2);
	createParams2.node = node2;

	auto scene1 = SceneCommon::CreateSceneOnSceneNode(createParams1);
	auto scene2 = SceneCommon::CreateSceneOnSceneNode(createParams2);

	EXPECT_EQ(1, tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneRegistryComp>(node1).GetTotalSceneCount());
	EXPECT_EQ(1, tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneRegistryComp>(node2).GetTotalSceneCount());

	EXPECT_EQ(2, SceneCommon::GetScenesSize());
	EXPECT_EQ(SceneCommon::GetScenesSize(), SceneCommon::GetScenesSize());

	sceneSystem.HandleDestroySceneNode(node1);

	EXPECT_FALSE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(node1));
	EXPECT_FALSE(tlsEcs.sceneRegistry.valid(scene1));
	EXPECT_TRUE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(node2));
	EXPECT_TRUE(tlsEcs.sceneRegistry.valid(scene2));

	EXPECT_EQ(1, tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneRegistryComp>(node2).GetTotalSceneCount());
	EXPECT_EQ(1, SceneCommon::GetScenesSize());
	EXPECT_EQ(0, SceneCommon::GetScenesSize(createParams1.sceneInfo.scene_confid()));
	EXPECT_EQ(1, SceneCommon::GetScenesSize(createParams2.sceneInfo.scene_confid()));

	sceneSystem.HandleDestroySceneNode(node2);

	EXPECT_EQ(0, SceneCommon::GetScenesSize());
	EXPECT_FALSE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(node1));
	EXPECT_FALSE(tlsEcs.sceneRegistry.valid(scene1));
	EXPECT_FALSE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(node2));
	EXPECT_FALSE(tlsEcs.sceneRegistry.valid(scene2));

	EXPECT_EQ(0, SceneCommon::GetScenesSize(createParams1.sceneInfo.scene_confid()));
	EXPECT_EQ(0, SceneCommon::GetScenesSize(createParams2.sceneInfo.scene_confid()));
	EXPECT_EQ(SceneCommon::GetScenesSize(), SceneCommon::GetScenesSize());
}

// ---------------------------------------------------------------------------
// 玩家进入/离开场景
// ---------------------------------------------------------------------------

TEST(SceneSystemTests, PlayerLeaveEnterScene)
{
	SceneSystem sceneSystem;

	auto node1 = CreateWorldNode();
	auto node2 = CreateWorldNode();

	CreateSceneOnNodeSceneParam createParams1;
	CreateSceneOnNodeSceneParam createParams2;

	createParams1.sceneInfo.set_scene_confid(3);
	createParams1.node = node1;

	createParams2.sceneInfo.set_scene_confid(2);
	createParams2.node = node2;

	auto scene1 = SceneCommon::CreateSceneOnSceneNode(createParams1);
	auto scene2 = SceneCommon::CreateSceneOnSceneNode(createParams2);

	EnterSceneParam enterParam1;
	enterParam1.scene = scene1;

	EnterSceneParam enterParam2;
	enterParam2.scene = scene2;

	uint32_t playerSize = 100;
	EntityUnorderedSet playerEntitySet1;
	EntityUnorderedSet playerEntitiesSet2;

	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto playerEntity = tlsEcs.actorRegistry.create();

		if (i % 2 == 0)
		{
			playerEntitySet1.emplace(playerEntity);
			enterParam1.enter = playerEntity;
			SceneCommon::EnterScene(enterParam1);
		}
		else
		{
			playerEntitiesSet2.emplace(playerEntity);
			enterParam2.enter = playerEntity;
			SceneCommon::EnterScene(enterParam2);
		}
	}

	const auto &scenesPlayers1 = tlsEcs.sceneRegistry.get<ScenePlayers>(scene1);
	const auto &scenesPlayers2 = tlsEcs.sceneRegistry.get<ScenePlayers>(scene2);

	for (const auto &playerEntity : playerEntitySet1)
	{
		EXPECT_TRUE(scenesPlayers1.find(playerEntity) != scenesPlayers1.end());
		EXPECT_TRUE(tlsEcs.actorRegistry.get_or_emplace<SceneEntityComp>(playerEntity).sceneEntity == scene1);
	}

	for (const auto &playerEntity : playerEntitiesSet2)
	{
		EXPECT_TRUE(scenesPlayers2.find(playerEntity) != scenesPlayers2.end());
		EXPECT_TRUE(tlsEcs.actorRegistry.get_or_emplace<SceneEntityComp>(playerEntity).sceneEntity == scene2);
	}

	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneNodePlayerStatsPtrComp>(node1)->player_size(), playerSize / 2);
	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneNodePlayerStatsPtrComp>(node2)->player_size(), playerSize / 2);

	LeaveSceneParam leaveParam1;
	for (const auto &playerEntity : playerEntitySet1)
	{
		leaveParam1.leaver = playerEntity;
		SceneCommon::LeaveScene(leaveParam1);
		EXPECT_FALSE(scenesPlayers1.find(playerEntity) != scenesPlayers1.end());
		EXPECT_EQ(tlsEcs.actorRegistry.try_get<SceneEntityComp>(playerEntity), nullptr);
	}

	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneNodePlayerStatsPtrComp>(node1)->player_size(), 0);

	LeaveSceneParam leaveParam2;
	for (const auto &playerEntity : playerEntitiesSet2)
	{
		leaveParam2.leaver = playerEntity;
		SceneCommon::LeaveScene(leaveParam2);
		EXPECT_FALSE(scenesPlayers2.find(playerEntity) != scenesPlayers2.end());
		EXPECT_EQ(tlsEcs.actorRegistry.try_get<SceneEntityComp>(playerEntity), nullptr);
	}

	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneNodePlayerStatsPtrComp>(node2)->player_size(), 0);

	auto &scenesPlayers11 = tlsEcs.sceneRegistry.get<ScenePlayers>(scene1);
	auto &scenesPlayers22 = tlsEcs.sceneRegistry.get<ScenePlayers>(scene2);
	EXPECT_TRUE(scenesPlayers11.empty());

	EXPECT_TRUE(scenesPlayers22.empty());

	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).destroy(node1);
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).destroy(node2);
}

// ---------------------------------------------------------------------------
// 节点维护模式下的场景分配
// ---------------------------------------------------------------------------

TEST(SceneNodeTest, MaintainModeWeightRoundRobinWorld)
{
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
	SceneSystem sm;
	SceneNodeSelectorSystem nodeSystem;
	EntityUnorderedSet serverEntities;
	const uint32_t serverSize = 2;
	const uint32_t perServerScene = 2;
	EntityUnorderedSet sceneEntities;

	for (uint32_t i = 0; i < serverSize; ++i)
	{
		serverEntities.emplace(CreateWorldNode());
	}

	CreateSceneOnNodeSceneParam createServerSceneParam;
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.sceneInfo.set_scene_confid(i);
		for (auto &it : serverEntities)
		{
			createServerSceneParam.node = it;
			auto scene = SceneCommon::CreateSceneOnSceneNode(createServerSceneParam);
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
		for (auto &&sceneEntity : sceneEntities)
		{
			enterParam1.enter = tlsEcs.actorRegistry.create();
			enterParam1.scene = sceneEntity;
			playerScene1.emplace(enterParam1.enter, enterParam1.scene);
			SceneCommon::EnterScene(enterParam1);
		}
	}
	SceneNodeStateSystem::MakeNodeState(*serverEntities.begin(), NodeState::kMaintain);

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

// ---------------------------------------------------------------------------
// 强制迁移玩家到其他场景
// ---------------------------------------------------------------------------

TEST(SceneNodeTest, CompelPlayerChangeScene)
{
	SceneSystem sm;

	auto node1 = CreateWorldNode();
	auto node2 = CreateWorldNode();

	CreateSceneOnNodeSceneParam server1Param;
	CreateSceneOnNodeSceneParam server2Param;

	server1Param.sceneInfo.set_scene_confid(2);
	server1Param.node = node1;

	server2Param.sceneInfo.set_scene_confid(2);
	server2Param.node = node2;

	const auto scene1 = SceneCommon::CreateSceneOnSceneNode(server1Param);
	const auto scene2 = SceneCommon::CreateSceneOnSceneNode(server2Param);

	EnterSceneParam enterParam1;
	enterParam1.scene = scene1;

	EnterSceneParam enterParam2;
	enterParam2.scene = scene2;

	constexpr uint32_t playerSize = 100;
	EntityUnorderedSet playerList1;
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto player = tlsEcs.actorRegistry.create();
		playerList1.emplace(player);
		enterParam1.enter = player;
		SceneCommon::EnterScene(enterParam1);
	}

	CompelChangeSceneParam compelChangeParam1;
	compelChangeParam1.destNode = node2;
	compelChangeParam1.sceneConfId = server2Param.sceneInfo.scene_confid();
	for (auto &it : playerList1)
	{
		compelChangeParam1.player = it;
		sm.CompelPlayerChangeScene(compelChangeParam1);
		EXPECT_TRUE(tlsEcs.actorRegistry.try_get<SceneEntityComp>(it)->sceneEntity == scene2);
	}
	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneNodePlayerStatsPtrComp>(node1)->player_size(), 0);
	EXPECT_EQ(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneNodePlayerStatsPtrComp>(node2)->player_size(), playerList1.size());
	auto &scenesPlayers11 = tlsEcs.sceneRegistry.get<ScenePlayers>(scene1);
	auto &scenesPlayers22 = tlsEcs.sceneRegistry.get<ScenePlayers>(scene2);
	EXPECT_TRUE(scenesPlayers11.empty());
	EXPECT_EQ(scenesPlayers22.size(), playerList1.size());
}

// ---------------------------------------------------------------------------
// 节点崩溃场景下的场景分配
// ---------------------------------------------------------------------------

TEST(SceneNodeTest, CrashModeWeightRoundRobinWorld)
{
	SceneSystem sm;
	EntityUnorderedSet serverEntities;
	uint32_t serverSize = 2;
	uint32_t perServerScene = 2;

	EntityUnorderedSet sceneEntities;

	for (uint32_t i = 0; i < serverSize; ++i)
	{
		serverEntities.emplace(CreateWorldNode());
	}

	CreateSceneOnNodeSceneParam createServerSceneParam;
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.sceneInfo.set_scene_confid(i);
		for (auto &it : serverEntities)
		{
			createServerSceneParam.node = it;
			auto e = SceneCommon::CreateSceneOnSceneNode(createServerSceneParam);
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
			auto pE = tlsEcs.actorRegistry.create();
			enterParam1.enter = pE;
			enterParam1.scene = it;
			playerScene1.emplace(enterParam1.enter, enterParam1.scene);
			SceneCommon::EnterScene(enterParam1);
		}
	}

	SceneNodeStateSystem::MakeNodeState(*serverEntities.begin(), NodeState::kCrash);

	uint32_t sceneConfigId0 = 0;
	GetSceneParams weightRoundRobinScene;
	weightRoundRobinScene.sceneConfigurationId = sceneConfigId0;
	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto canEnter = SceneNodeSelectorSystem::SelectLeastLoadedScene(weightRoundRobinScene);
		EXPECT_TRUE(canEnter != entt::null);
	}
}

// 节点崩溃时的消息不能被处理
TEST(SceneNodeTest, CrashNodeReplaceMovePlayersToNewNode)
{
	SceneSystem sm;
	EntityUnorderedSet nodeList;
	EntityUnorderedSet sceneList;
	uint32_t nodeSize = 2;
	uint32_t perNodeScene = 2;
	entt::entity firstScene = entt::null;

	for (uint32_t i = 0; i < nodeSize; ++i)
	{
		nodeList.emplace(CreateWorldNode());
	}

	CreateSceneOnNodeSceneParam createNodeSceneParam;
	for (uint32_t i = 0; i < perNodeScene; ++i)
	{
		createNodeSceneParam.sceneInfo.set_scene_confid(i);
		for (auto &it : nodeList)
		{
			createNodeSceneParam.node = it;
			auto e = SceneCommon::CreateSceneOnSceneNode(createNodeSceneParam);
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
		auto player = tlsEcs.actorRegistry.create();
		enterParam1.enter = player;
		enterParam1.scene = firstScene;
		playerScene1.emplace(enterParam1.enter, enterParam1.scene);
		SceneCommon::EnterScene(enterParam1);
	}

	SceneNodeStateSystem::MakeNodeState(*nodeList.begin(), NodeState::kCrash);

	entt::entity crashNode = *nodeList.begin();
	entt::entity replaceNode = *(++nodeList.begin());
	sm.ReplaceCrashSceneNode(crashNode, replaceNode);

	EXPECT_FALSE(tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(crashNode));
	nodeList.erase(crashNode);
	for (auto &it : nodeList)
	{
		auto &serverScene = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneRegistryComp>(it);
		EXPECT_EQ(serverScene.GetTotalSceneCount(), sceneList.size());
	}
}

// ---------------------------------------------------------------------------
// 权重轮循场景分配
// ---------------------------------------------------------------------------

TEST(SceneNodeTest, WeightRoundRobinWorld)
{
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
	SceneSystem sm;
	EntityUnorderedSet node_list;
	uint32_t server_size = 10;
	uint32_t per_server_scene = 10;
	for (uint32_t i = 0; i < server_size; ++i)
	{

		node_list.emplace(CreateWorldNode());
	}

	CreateSceneOnNodeSceneParam create_server_scene_param;

	for (uint32_t i = 0; i < per_server_scene; ++i)
	{
		create_server_scene_param.sceneInfo.set_scene_confid(i);
		for (auto &it : node_list)
		{
			create_server_scene_param.node = it;
			SceneCommon::CreateSceneOnSceneNode(create_server_scene_param);
		}
	}

	auto enter_leave_lambda = [&node_list, server_size, per_server_scene, &sm]() -> void
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
			auto can_enter = SceneNodeSelectorSystem::SelectLeastLoadedScene(weight_round_robin_scene);
			auto p_e = tlsEcs.actorRegistry.create();
			enter_param1.enter = p_e;
			enter_param1.scene = can_enter;
			player_scene1.emplace(enter_param1.enter, can_enter);
			scene_sets.emplace(can_enter);
			SceneCommon::EnterScene(enter_param1);
		}

		uint32_t player_scene_guid = 0;
		for (auto &it : player_scene1)
		{
			auto &pse = tlsEcs.actorRegistry.get_or_emplace<SceneEntityComp>(it.first);
			EXPECT_TRUE(pse.sceneEntity == it.second);
			EXPECT_EQ(tlsEcs.sceneRegistry.get<SceneInfoComp>(pse.sceneEntity).scene_confid(), scene_config_id0);
		}

		std::unordered_map<entt::entity, entt::entity> player_scene2;
		weight_round_robin_scene.sceneConfigurationId = scene_config_id1;
		for (uint32_t i = 0; i < player_size; ++i)
		{
			auto can_enter = SceneNodeSelectorSystem::SelectLeastLoadedScene(weight_round_robin_scene);
			auto player = tlsEcs.actorRegistry.create();
			enter_param1.enter = player;
			enter_param1.scene = can_enter;
			player_scene2.emplace(enter_param1.enter, enter_param1.scene);
			scene_sets.emplace(can_enter);
			SceneCommon::EnterScene(enter_param1);
		}
		player_scene_guid = 0;
		for (auto &it : player_scene2)
		{
			auto &pse = tlsEcs.actorRegistry.get_or_emplace<SceneEntityComp>(it.first);
			EXPECT_TRUE(pse.sceneEntity == it.second);
			EXPECT_EQ(tlsEcs.sceneRegistry.get<SceneInfoComp>(pse.sceneEntity).scene_confid(), scene_config_id1);
		}

		std::size_t server_player_size = player_size * 2 / server_size;

		for (auto &it : node_list)
		{
			auto &ps = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneNodePlayerStatsPtrComp>(it);
			EXPECT_EQ((*ps).player_size(), server_player_size);
		}
		EXPECT_EQ(scene_sets.size(), std::size_t(2 * per_server_scene));

		LeaveSceneParam leave_scene;
		for (auto &it : player_scene1)
		{
			auto &pse = tlsEcs.actorRegistry.get_or_emplace<SceneEntityComp>(it.first);
			leave_scene.leaver = it.first;
			SceneCommon::LeaveScene(leave_scene);
		}
		for (auto &it : player_scene2)
		{
			auto &pse = tlsEcs.actorRegistry.get_or_emplace<SceneEntityComp>(it.first);
			leave_scene.leaver = it.first;
			SceneCommon::LeaveScene(leave_scene);
		}
		for (auto &it : node_list)
		{
			auto &ps = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneNodePlayerStatsPtrComp>(it);
			EXPECT_EQ((*ps).player_size(), 0);
		}
		for (auto &it : player_scene1)
		{
			EXPECT_EQ(tlsEcs.sceneRegistry.get<ScenePlayers>(it.second).size(), 0);
		}
		for (auto &it : player_scene2)
		{
			EXPECT_EQ(tlsEcs.sceneRegistry.get<ScenePlayers>(it.second).size(), 0);
		}
	};
	for (uint32_t i = 0; i < 2; ++i)
	{
		enter_leave_lambda();
	}
	// leave
}

// ---------------------------------------------------------------------------
// 节点压力状态下的场景分配
// ---------------------------------------------------------------------------

TEST(SceneNodeTest, PressureModeEnterLeave)
{
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
	SceneSystem sm;
	EntityUnorderedSet serverEntities;
	uint32_t serverSize = 2;
	uint32_t perServerScene = 10;

	// Create server nodes
	for (uint32_t i = 0; i < serverSize; ++i)
	{
		serverEntities.emplace(CreateWorldNode());
	}

	// Create scenes on each server node
	CreateSceneOnNodeSceneParam createServerSceneParam;
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.sceneInfo.set_scene_confid(i);
		for (auto &it : serverEntities)
		{
			createServerSceneParam.node = it;
			SceneCommon::CreateSceneOnSceneNode(createServerSceneParam);
		}
	}

	// Set pressure on the first server node
	SceneNodeStateSystem::MakeNodePressure(*serverEntities.begin());

	uint32_t sceneConfigId0 = 0;
	uint32_t sceneConfigId1 = 1;

	GetSceneParams weightRoundRobinScene;
	weightRoundRobinScene.sceneConfigurationId = sceneConfigId0;

	std::unordered_map<entt::entity, entt::entity> playerScene1;
	EnterSceneParam enterParam1;

	// Enter players into scenes with sceneConfigId0
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		auto canEnter = SceneNodeSelectorSystem::SelectLeastLoadedScene(weightRoundRobinScene);
		auto playerEntity = tlsEcs.actorRegistry.create();
		enterParam1.enter = playerEntity;
		enterParam1.scene = canEnter;
		playerScene1.emplace(enterParam1.enter, enterParam1.scene);
		SceneCommon::EnterScene(enterParam1);
	}

	// Clear pressure on the first server node
	SceneNodeStateSystem::ClearNodePressure(*serverEntities.begin());

	std::unordered_map<entt::entity, entt::entity> playerScene2;
	weightRoundRobinScene.sceneConfigurationId = sceneConfigId1;

	// Enter players into scenes with sceneConfigId1
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		auto canEnter = SceneNodeSelectorSystem::SelectLeastLoadedScene(weightRoundRobinScene);
		auto playerEntity = tlsEcs.actorRegistry.create();
		enterParam1.enter = playerEntity;
		enterParam1.scene = canEnter;
		playerScene2.emplace(enterParam1.enter, enterParam1.scene);
		SceneCommon::EnterScene(enterParam1);
	}
}

// ---------------------------------------------------------------------------
// 进入默认场景
// ---------------------------------------------------------------------------

TEST(SceneNodeTest, EnterDefaultScene)
{
	const auto gameNode = CreateWorldNode();
	CreateSceneOnNodeSceneParam createGSSceneParam{gameNode};

	// Create multiple scenes for the game node
	for (uint32_t i = 1; i < kConfigSceneListSize; ++i)
	{
		createGSSceneParam.sceneInfo.set_scene_confid(i);
		for (uint32_t j = 0; j < kPerSceneConfigSize; ++j)
		{
			SceneCommon::CreateSceneOnSceneNode(createGSSceneParam);
		}
	}

	// Create a player entity
	const auto player = tlsEcs.actorRegistry.create();

	// Enter the default scene with the player
	const EnterDefaultSceneParam enterParam{player};
	SceneSystem::EnterDefaultScene(enterParam);

	// Verify the player is in the default scene
	const auto [sceneEntity] = tlsEcs.actorRegistry.get_or_emplace<SceneEntityComp>(player);
	const auto &sceneInfo = tlsEcs.sceneRegistry.get<SceneInfoComp>(sceneEntity);
	EXPECT_EQ(sceneInfo.scene_confid(), kDefaultSceneId);
}

struct TestNodeId
{
	uint32_t node_id_{0};
};

// ---------------------------------------------------------------------------
// 场景人数满时的场景分配
// ---------------------------------------------------------------------------

TEST(SceneNodeTest, GetNotFullWorldWhenSceneFull)
{
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
	SceneSystem sm;
	SceneNodeSelectorSystem nssys;
	EntityUnorderedSet serverEntities;
	uint32_t serverSize = 10;
	uint32_t perServerScene = 10;

	// Create server entities and assign node IDs
	for (uint32_t i = 0; i < serverSize; ++i)
	{
		auto server = CreateWorldNode();
		serverEntities.emplace(server);
		tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get_or_emplace<TestNodeId>(server).node_id_ = i;
	}

	CreateSceneOnNodeSceneParam createServerSceneParam;

	// Create scenes for each server entity
	for (uint32_t i = 0; i < perServerScene; ++i)
	{
		createServerSceneParam.sceneInfo.set_scene_confid(i);
		for (auto &it : serverEntities)
		{
			createServerSceneParam.node = it;
			auto scene1 = SceneCommon::CreateSceneOnSceneNode(createServerSceneParam);
			tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get_or_emplace<TestNodeId>(scene1, tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<TestNodeId>(it));
			auto scene2 = SceneCommon::CreateSceneOnSceneNode(createServerSceneParam);
			tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get_or_emplace<TestNodeId>(scene2, tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<TestNodeId>(it));
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
			auto canEnter = nssys.SelectAvailableScene(weightRoundRobinScene);
			if (canEnter == entt::null)
			{
				continue;
			}
			auto playerEntity = tlsEcs.actorRegistry.create();
			enterParam1.enter = playerEntity;
			enterParam1.scene = canEnter;
			playerScene1.emplace(enterParam1.enter, canEnter);
			sceneSets.emplace(canEnter);
			SceneCommon::EnterScene(enterParam1);
		}

		// Verify players are correctly placed in scenes and sceneConfigId0 is assigned
		for (auto &it : playerScene1)
		{
			auto &pse = tlsEcs.actorRegistry.get_or_emplace<SceneEntityComp>(it.first);
			EXPECT_TRUE(pse.sceneEntity == it.second);
			EXPECT_EQ(tlsEcs.sceneRegistry.get<SceneInfoComp>(pse.sceneEntity).scene_confid(), sceneConfigId0);
		}

		// Enter players into scenes with sceneConfigId1
		std::unordered_map<entt::entity, entt::entity> playerScene2;
		weightRoundRobinScene.sceneConfigurationId = sceneConfigId1;
		for (uint32_t i = 0; i < playerSize; ++i)
		{
			auto canEnter = nssys.SelectAvailableScene(weightRoundRobinScene);
			if (canEnter == entt::null)
			{
				continue;
			}
			auto playerEntity = tlsEcs.actorRegistry.create();
			enterParam1.enter = playerEntity;
			enterParam1.scene = canEnter;
			playerScene2.emplace(enterParam1.enter, enterParam1.scene);
			sceneSets.emplace(canEnter);
			SceneCommon::EnterScene(enterParam1);
		}

		// Verify players are correctly placed in scenes and sceneConfigId1 is assigned
		for (auto &it : playerScene2)
		{
			auto &pse = tlsEcs.actorRegistry.get_or_emplace<SceneEntityComp>(it.first);
			EXPECT_TRUE(pse.sceneEntity == it.second);
			EXPECT_EQ(tlsEcs.sceneRegistry.get<SceneInfoComp>(pse.sceneEntity).scene_confid(), sceneConfigId1);
		}

		// Calculate expected player distribution across servers
		std::size_t serverPlayerSize = playerSize * 2 / serverSize;
		std::size_t remainServerSize = playerSize * 2 - kMaxPlayersPerScene * 2;

		// Verify player distribution across server entities
		for (auto &it : serverEntities)
		{
			auto &ps = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneNodePlayerStatsPtrComp>(it);
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
		LeaveSceneParam leaveScene;
		for (auto &it : playerScene1)
		{
			leaveScene.leaver = it.first;
			SceneCommon::LeaveScene(leaveScene);
		}

		// Leave scenes for playerScene2
		for (auto &it : playerScene2)
		{
			leaveScene.leaver = it.first;
			SceneCommon::LeaveScene(leaveScene);
		}

		// Verify all server entities have no players after leaving scenes
		for (auto &it : serverEntities)
		{
			auto &ps = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<SceneNodePlayerStatsPtrComp>(it);
			EXPECT_EQ((*ps).player_size(), 0);
		}

		// Verify no scenes have players after leaving scenes
		for (auto &it : playerScene1)
		{
			EXPECT_EQ(tlsEcs.sceneRegistry.get<ScenePlayers>(it.second).size(), 0);
		}
		for (auto &it : playerScene2)
		{
			EXPECT_EQ(tlsEcs.sceneRegistry.get<ScenePlayers>(it.second).size(), 0);
		}
	};

	// Execute enterLeaveLambda twice to simulate multiple test iterations
	for (uint32_t i = 0; i < 2; ++i)
	{
		enterLeaveLambda();
	}
}

TEST(SceneNodeTest, CreateDungeon)
{
}

TEST(SceneNodeTest, Route)
{
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
}

// ---------------------------------------------------------------------------
// 场景进入条件检查
// ---------------------------------------------------------------------------

TEST(SceneNodeTest, CheckEnterSceneWithCreators)
{
	// Create a scene with creators set
	SceneInfoComp sceneInfo;
	for (uint64_t i = 1; i < 10; ++i)
	{
		sceneInfo.mutable_creators()->emplace(i, false); // Assuming creators are added with a boolean indicating creator status
	}
	auto scene = SceneCommon::CreateSceneOnSceneNode({.node = CreateWorldNode(), .sceneInfo = sceneInfo});

	// Create players with different GUIDs
	const auto player1 = tlsEcs.actorRegistry.create();
	tlsEcs.actorRegistry.emplace<Guid>(player1, 1); // Player 1 with GUID 1
	const auto player2 = tlsEcs.actorRegistry.create();
	tlsEcs.actorRegistry.emplace<Guid>(player2, 100); // Player 2 with GUID 100

	// Test cases
	EXPECT_EQ(kSuccess, SceneCommon::CheckPlayerEnterScene({.scene = scene, .enter = player1}));
	EXPECT_EQ(kCheckEnterSceneCreator, SceneCommon::CheckPlayerEnterScene({.scene = scene, .enter = player2}));
}

int32_t main(int argc, char **argv)
{
	muduo::Logger::setLogLevel(muduo::Logger::ERROR_);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
