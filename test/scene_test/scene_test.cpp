#include <gtest/gtest.h>

#include "system/scene/scene_system.h"
#include "comp/scene.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"

#include "proto/logic/component/gs_node_comp.pb.h"
#include "proto/logic/component/scene_comp.pb.h"

using GameNodeInfoPtr = std::shared_ptr<GameNodeInfo>;

const std::size_t kConfigSceneListSize = 50;
const std::size_t kPerSceneConfigSize = 2;

entt::entity CreateMainSceneNode()
{
	const auto node = tls.game_node_registry.create();
	AddMainSceneNodeComponent(tls.game_node_registry, node);
	return node;
}

TEST(SceneSystemTests, CreateMainScene)
{
	const ScenesSystem sceneSystem;

	CreateGameNodeSceneParam createParams;
	const auto serverEntity1 = CreateMainSceneNode();

	createParams.node = serverEntity1;
	for (uint32_t i = 0; i < kConfigSceneListSize; ++i)
	{
		createParams.sceneInfo.set_scene_confid(i);
		for (uint32_t j = 0; j < kPerSceneConfigSize; ++j)
		{
			sceneSystem.CreateScene2GameNode(createParams);
		}
		EXPECT_EQ(sceneSystem.GetScenesSize(i), kPerSceneConfigSize);
	}
	EXPECT_EQ(sceneSystem.GetScenesSize(), kConfigSceneListSize * kPerSceneConfigSize);
}

TEST(SceneSystemTests, CreateScene2Server)
{
	ScenesSystem sceneSystem;
	const auto node1 = CreateMainSceneNode();
	const auto node2 = CreateMainSceneNode();

	CreateGameNodeSceneParam createParams1;
	CreateGameNodeSceneParam createParams2;

	createParams1.sceneInfo.set_scene_confid(2);
	createParams1.node = node1;

	createParams2.sceneInfo.set_scene_confid(3);
	createParams2.node = node2;

	sceneSystem.CreateScene2GameNode(createParams1);
	sceneSystem.CreateScene2GameNode(createParams2);

	const auto nodeComp1 = tls.game_node_registry.try_get<NodeSceneComp>(node1);
	if (nodeComp1)
	{
		EXPECT_EQ(1, nodeComp1->GetTotalSceneCount());
	}

	const auto nodeComp2 = tls.game_node_registry.try_get<NodeSceneComp>(node2);
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
	ScenesSystem sceneSystem;
	const auto node1 = CreateMainSceneNode();

	CreateGameNodeSceneParam createParams1;
	createParams1.node = node1;
	const auto scene = sceneSystem.CreateScene2GameNode(createParams1);

	EXPECT_EQ(1, sceneSystem.GetScenesSize());
	EXPECT_EQ(1, sceneSystem.GetScenesSize(createParams1.sceneInfo.scene_confid()));

	auto serverComp1 = tls.game_node_registry.try_get<NodeSceneComp>(node1);
	if (serverComp1)
	{
		EXPECT_EQ(1, serverComp1->GetTotalSceneCount());
	}

	sceneSystem.DestroyScene({ node1, scene });

	EXPECT_TRUE(sceneSystem.IsSceneEmpty());
	EXPECT_FALSE(sceneSystem.ConfigSceneListNotEmpty(createParams1.sceneInfo.scene_confid()));
	EXPECT_TRUE(sceneSystem.IsSceneEmpty());
	EXPECT_EQ(sceneSystem.GetScenesSize(), sceneSystem.GetScenesSize());
	EXPECT_FALSE(tls.game_node_registry.valid(scene));
}

TEST(SceneSystemTests, DestroyServer)
{
	ScenesSystem sceneSystem;

	auto node1 = CreateMainSceneNode();
	auto node2 = CreateMainSceneNode();

	CreateGameNodeSceneParam createParams1;
	CreateGameNodeSceneParam createParams2;

	createParams1.sceneInfo.set_scene_confid(3);
	createParams1.node = node1;

	createParams2.sceneInfo.set_scene_confid(2);
	createParams2.node = node2;

	auto scene1 = sceneSystem.CreateScene2GameNode(createParams1);
	auto scene2 = sceneSystem.CreateScene2GameNode(createParams2);

	EXPECT_EQ(1, tls.game_node_registry.get<NodeSceneComp>(node1).GetTotalSceneCount());
	EXPECT_EQ(1, tls.game_node_registry.get<NodeSceneComp>(node2).GetTotalSceneCount());

	EXPECT_EQ(2, sceneSystem.GetScenesSize());
	EXPECT_EQ(sceneSystem.GetScenesSize(), sceneSystem.GetScenesSize());

	sceneSystem.OnDestroyServer(node1);

	EXPECT_FALSE(tls.game_node_registry.valid(node1));
	EXPECT_FALSE(tls.scene_registry.valid(scene1));
	EXPECT_TRUE(tls.game_node_registry.valid(node2));
	EXPECT_TRUE(tls.scene_registry.valid(scene2));

	EXPECT_EQ(1, tls.game_node_registry.get<NodeSceneComp>(node2).GetTotalSceneCount());
	EXPECT_EQ(1, sceneSystem.GetScenesSize());
	EXPECT_EQ(0, sceneSystem.GetScenesSize(createParams1.sceneInfo.scene_confid()));
	EXPECT_EQ(1, sceneSystem.GetScenesSize(createParams2.sceneInfo.scene_confid()));

	sceneSystem.OnDestroyServer(node2);

	EXPECT_EQ(0, sceneSystem.GetScenesSize());
	EXPECT_FALSE(tls.game_node_registry.valid(node1));
	EXPECT_FALSE(tls.scene_registry.valid(scene1));
	EXPECT_FALSE(tls.game_node_registry.valid(node2));
	EXPECT_FALSE(tls.scene_registry.valid(scene2));

	EXPECT_EQ(0, sceneSystem.GetScenesSize(createParams1.sceneInfo.scene_confid()));
	EXPECT_EQ(0, sceneSystem.GetScenesSize(createParams2.sceneInfo.scene_confid()));
	EXPECT_EQ(sceneSystem.GetScenesSize(), sceneSystem.GetScenesSize());
}

TEST(SceneSystemTests, PlayerLeaveEnterScene)
{
	ScenesSystem sceneSystem;

	auto node1 = CreateMainSceneNode();
	auto node2 = CreateMainSceneNode();

	CreateGameNodeSceneParam createParams1;
	CreateGameNodeSceneParam createParams2;

	createParams1.sceneInfo.set_scene_confid(3);
	createParams1.node = node1;

	createParams2.sceneInfo.set_scene_confid(2);
	createParams2.node = node2;

	auto scene1 = sceneSystem.CreateScene2GameNode(createParams1);
	auto scene2 = sceneSystem.CreateScene2GameNode(createParams2);

	EnterSceneParam enterParam1;
	enterParam1.scene = scene1;

	EnterSceneParam enterParam2;
	enterParam2.scene = scene2;

	uint32_t playerSize = 100;
	EntitySet playerEntitySet1;
	EntitySet playerEntitiesSet2;

	for (uint32_t i = 0; i < playerSize; ++i)
	{
		auto playerEntity = tls.game_node_registry.create();

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

	const auto& scenesPlayers1 = tls.scene_registry.get<ScenePlayers>(scene1);
	const auto& scenesPlayers2 = tls.scene_registry.get<ScenePlayers>(scene2);

	for (const auto& playerEntity : playerEntitySet1)
	{
		EXPECT_TRUE(scenesPlayers1.find(playerEntity) != scenesPlayers1.end());
		EXPECT_TRUE(tls.registry.get<SceneEntity>(playerEntity).sceneEntity == scene1);
	}

	for (const auto& playerEntity : playerEntitiesSet2)
	{
		EXPECT_TRUE(scenesPlayers2.find(playerEntity) != scenesPlayers2.end());
		EXPECT_TRUE(tls.registry.get<SceneEntity>(playerEntity).sceneEntity == scene2);
	}

	EXPECT_EQ(tls.game_node_registry.get<GameNodeInfoPtr>(node1)->player_size(), playerSize / 2);
	EXPECT_EQ(tls.game_node_registry.get<GameNodeInfoPtr>(node2)->player_size(), playerSize / 2);

	LeaveSceneParam leaveParam1;
	for (const auto& playerEntity : playerEntitySet1)
	{
		leaveParam1.leaver = playerEntity;
		sceneSystem.LeaveScene(leaveParam1);
		EXPECT_FALSE(scenesPlayers1.find(playerEntity) != scenesPlayers1.end());
		EXPECT_EQ(tls.registry.try_get<SceneEntity>(playerEntity), nullptr);
	}

	EXPECT_EQ(tls.game_node_registry.get<GameNodeInfoPtr>(node1)->player_size(), 0);

	LeaveSceneParam leaveParam2;
	for (const auto& playerEntity : playerEntitiesSet2)
	{
		leaveParam2.leaver = playerEntity;
		sceneSystem.LeaveScene(leaveParam2);
		EXPECT_FALSE(scenesPlayers2.find(playerEntity) != scenesPlayers2.end());
		EXPECT_EQ(tls.registry.try_get<SceneEntity>(playerEntity), nullptr);
	}

	EXPECT_EQ(tls.game_node_registry.get<GameNodeInfoPtr>(node2)->player_size(), 0);

	auto& scenesPlayers11 = tls.scene_registry.get<ScenePlayers>(scene1);
	auto& scenesPlayers22 = tls.scene_registry.get<ScenePlayers>(scene2);
	EXPECT_TRUE(scenesPlayers11.empty());


	EXPECT_TRUE(scenesPlayers22.empty());

	tls.game_node_registry.destroy(node1);
	tls.game_node_registry.destroy(node2);
}

TEST(GS, MainTainWeightRoundRobinMainScene)
{
    tls.game_node_registry.clear();
    ScenesSystem sm;
    NodeSceneSystem node_system;
    EntitySet server_entities;
    const uint32_t server_size = 2;
    const uint32_t per_server_scene = 2;
    EntitySet scene_entities;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        server_entities.emplace(CreateMainSceneNode());
    }

    CreateGameNodeSceneParam create_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        create_server_scene_param.sceneInfo.set_scene_confid(i);
        for (auto& it : server_entities)
        {
            create_server_scene_param.node = it;
            auto scene = sm.CreateScene2GameNode(create_server_scene_param);
            if (scene_entities.empty())
            {
                scene_entities.emplace(scene);
            }
        }
    }

    constexpr uint32_t player_size = 1000;

    std::unordered_map<entt::entity, entt::entity> player_scene1;

    EnterSceneParam enter_param1;
    //todo
    // 进入第一个场景 
    for (uint32_t i = 0; i < player_size; ++i)
    {
        for (auto&& sceneEntity : scene_entities)
        {
            enter_param1.enter = tls.game_node_registry.create();
            enter_param1.scene = sceneEntity;
            player_scene1.emplace(enter_param1.enter, enter_param1.scene);
            sm.EnterScene(enter_param1);
        }
    }
    NodeSceneSystem::SetNodeState(*server_entities.begin(), NodeState::kMaintain);

    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.sceneConfId = 0;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = node_system.FindSceneWithMinPlayerCount(weight_round_robin_scene);
        EXPECT_TRUE(can_enter != entt::null);
    }

    weight_round_robin_scene.sceneConfId = 1;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = node_system.FindSceneWithMinPlayerCount(weight_round_robin_scene);
        EXPECT_TRUE(can_enter != entt::null);
    }
}

TEST(GS, CompelToChangeScene)
{
    ScenesSystem sm;

    auto node1 = CreateMainSceneNode();
    auto node2 = CreateMainSceneNode();

    CreateGameNodeSceneParam server1_param;
    CreateGameNodeSceneParam server2_param;

    server1_param.sceneInfo.set_scene_confid( 2);
    server1_param.node = node1;

    server2_param.sceneInfo.set_scene_confid( 2);
    server2_param.node = node2;

    const auto scene1 = sm.CreateScene2GameNode(server1_param);
    const auto scene2 = sm.CreateScene2GameNode(server2_param);

    EnterSceneParam enter_param1;
    enter_param1.scene = scene1;

    EnterSceneParam enter_param2;
    enter_param2.scene = scene2;

    constexpr uint32_t player_size = 100;
    EntitySet player_list1;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto player = tls.game_node_registry.create();
        player_list1.emplace(player);
        enter_param1.enter = player;
        sm.EnterScene(enter_param1);
    }

    CompelChangeSceneParam compel_change_param1;
    compel_change_param1.destNode = node2;
    compel_change_param1.sceneConfId = server2_param.sceneInfo.scene_confid();
    for (auto& it : player_list1)
    {
        compel_change_param1.player = it;
        sm.CompelPlayerChangeScene(compel_change_param1);
        EXPECT_TRUE(tls.registry.try_get<SceneEntity>(it)->sceneEntity == scene2);
    }
    EXPECT_EQ(tls.game_node_registry.get<GameNodeInfoPtr>(node1)->player_size(), 0);
    EXPECT_EQ(tls.game_node_registry.get<GameNodeInfoPtr>(node2)->player_size(), player_list1.size());
    auto& scenes_players11 = tls.scene_registry.get<ScenePlayers>(scene1);
    auto& scenes_players22 = tls.scene_registry.get<ScenePlayers>(scene2);
    EXPECT_TRUE(scenes_players11.empty());
    EXPECT_EQ(scenes_players22.size(), player_list1.size());
}


TEST(GS, CrashWeightRoundRobinMainScene)
{
    ScenesSystem sm;
    NodeSceneSystem nssys;
    EntitySet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 2;

    EntitySet scene_entities;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        server_entities.emplace(CreateMainSceneNode( ));
    }

    CreateGameNodeSceneParam create_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        create_server_scene_param.sceneInfo.set_scene_confid(i);
        for (auto& it : server_entities)
        {
            create_server_scene_param.node = it;
            auto e = sm.CreateScene2GameNode(create_server_scene_param);
            if (scene_entities.empty())
            {
                scene_entities.emplace(e);
            }
        }
    }

    uint32_t player_size = 1000;

    std::unordered_map<entt::entity, entt::entity> player_scene1;

    EnterSceneParam enter_param1;

    for (uint32_t i = 0; i < player_size; ++i)
    {
        for (auto it : scene_entities)
        {
            auto p_e = tls.game_node_registry.create();
            enter_param1.enter = p_e;
            enter_param1.scene = it;
            player_scene1.emplace(enter_param1.enter, enter_param1.scene);
            sm.EnterScene(enter_param1);
        }
    }


    nssys.SetNodeState(*server_entities.begin(), NodeState::kCrash);

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;
    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.sceneConfId = scene_config_id0;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = nssys.FindSceneWithMinPlayerCount(weight_round_robin_scene);
        EXPECT_TRUE(can_enter != entt::null);
    }

}

//崩溃时候的消息不能处理
TEST(GS, CrashMovePlayer2NewServer)
{
    ScenesSystem sm;
    NodeSceneSystem nssys;
    EntitySet node_list;
    EntitySet scene_list;
    uint32_t node_size = 2;
    uint32_t per_node_scene = 2;
    entt::entity first_scene = entt::null;

    for (uint32_t i = 0; i < node_size; ++i)
    {
        node_list.emplace(CreateMainSceneNode());
    }

    CreateGameNodeSceneParam create_node_scene_param;
    for (uint32_t i = 0; i < per_node_scene; ++i)
    {
        create_node_scene_param.sceneInfo.set_scene_confid(i);
        for (auto& it : node_list)
        {
            create_node_scene_param.node = it;
            auto e = sm.CreateScene2GameNode(create_node_scene_param);
            scene_list.emplace(e);
            if (first_scene == entt::null)
            {
                first_scene = e;
            }
        }
    }

    uint32_t player_size = 1000;

    std::unordered_map<entt::entity, entt::entity> player_scene1;

    EnterSceneParam enter_param1;

    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto player = tls.game_node_registry.create();
        enter_param1.enter = player;
        enter_param1.scene = first_scene;
        player_scene1.emplace(enter_param1.enter, enter_param1.scene);
        sm.EnterScene(enter_param1);
    }
    
    nssys.SetNodeState(*node_list.begin(), NodeState::kCrash);

   
    entt::entity cransh_node = *node_list.begin();
     entt::entity replace_node = *(++node_list.begin());
    sm.ReplaceCrashServer(cransh_node, replace_node);

    EXPECT_FALSE(tls.game_node_registry.valid(cransh_node));
    node_list.erase(cransh_node);
    for (auto& it : node_list)
    {
        auto& server_scene =  tls.game_node_registry.get<NodeSceneComp>(it);
        EXPECT_EQ(server_scene.GetTotalSceneCount(), scene_list.size());
    }
    
}

TEST(GS, WeightRoundRobinMainScene)
{
    tls.game_node_registry.clear();
    ScenesSystem sm;
    NodeSceneSystem nssys;
    EntitySet node_list;
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
        for (auto& it :node_list)
        {
            create_server_scene_param.node = it;
            sm.CreateScene2GameNode(create_server_scene_param);
        }        
    }

    auto enter_leave_lambda = [&node_list, server_size, per_server_scene, &sm, &nssys]()->void
    {
        uint32_t scene_config_id0 = 0;
        uint32_t scene_config_id1 = 1;
        GetSceneParam weight_round_robin_scene;
        weight_round_robin_scene.sceneConfId = scene_config_id0;

        uint32_t player_size = 1000;

        std::unordered_map<entt::entity, entt::entity> player_scene1;
        EnterSceneParam enter_param1;

        EntitySet scene_sets;

        for (uint32_t i = 0; i < player_size; ++i)
        {
            auto can_enter = nssys.FindSceneWithMinPlayerCount(weight_round_robin_scene);
            auto p_e = tls.game_node_registry.create();
            enter_param1.enter = p_e;
            enter_param1.scene = can_enter;
            player_scene1.emplace(enter_param1.enter, can_enter);
            scene_sets.emplace(can_enter);
            sm.EnterScene(enter_param1);
        }

        uint32_t player_scene_guid = 0;
        for (auto& it : player_scene1)
        {
            auto& pse = tls.registry.get<SceneEntity>(it.first);
            EXPECT_TRUE(pse.sceneEntity == it.second);
            EXPECT_EQ(tls.scene_registry.get<SceneInfo>(pse.sceneEntity).scene_confid(), scene_config_id0);
        }

        std::unordered_map<entt::entity, entt::entity> player_scene2;
        weight_round_robin_scene.sceneConfId = scene_config_id1;
        for (uint32_t i = 0; i < player_size; ++i)
        {
            auto can_enter = nssys.FindSceneWithMinPlayerCount(weight_round_robin_scene);
            auto player = tls.game_node_registry.create();
            enter_param1.enter = player;
            enter_param1.scene = can_enter;
            player_scene2.emplace(enter_param1.enter, enter_param1.scene);
            scene_sets.emplace(can_enter);
            sm.EnterScene(enter_param1);
        }
        player_scene_guid = 0;
        for (auto& it : player_scene2)
        {
            auto& pse = tls.registry.get<SceneEntity>(it.first);
            EXPECT_TRUE(pse.sceneEntity == it.second);
            EXPECT_EQ(tls.scene_registry.get<SceneInfo>(pse.sceneEntity).scene_confid(), scene_config_id1);
        }

        std::size_t server_player_size = player_size * 2 / server_size;


        for (auto& it : node_list)
        {
            auto& ps = tls.game_node_registry.get<GameNodeInfoPtr>(it);
            EXPECT_EQ((*ps).player_size(), server_player_size);
        }
        EXPECT_EQ(scene_sets.size(), std::size_t(2 * per_server_scene));

        LeaveSceneParam leave_scene;
        for (auto& it : player_scene1)
        {
            auto& pse = tls.registry.get<SceneEntity>(it.first);
            leave_scene.leaver = it.first;
            sm.LeaveScene(leave_scene);
        }
        for (auto& it : player_scene2)
        {
            auto& pse = tls.registry.get<SceneEntity>(it.first);
            leave_scene.leaver = it.first;
            sm.LeaveScene(leave_scene);
        }
        for (auto& it : node_list)
        {
            auto& ps = tls.game_node_registry.get<GameNodeInfoPtr>(it);
            EXPECT_EQ((*ps).player_size(), 0);
        }
        for (auto& it : player_scene1)
        {
            EXPECT_EQ(tls.scene_registry.get<ScenePlayers>(it.second).size(), 0);
        }
        for (auto& it : player_scene2)
        {
            EXPECT_EQ(tls.scene_registry.get<ScenePlayers>(it.second).size(), 0);
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
    tls.game_node_registry.clear();
    ScenesSystem sm;
    NodeSceneSystem nssys;
    EntitySet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 10;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        server_entities.emplace(CreateMainSceneNode());
    }

    CreateGameNodeSceneParam create_server_scene_param;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        create_server_scene_param.sceneInfo.set_scene_confid(i);
        for (auto& it : server_entities)
        {
            create_server_scene_param.node = it;
            sm.CreateScene2GameNode(create_server_scene_param);
        }
    }

    nssys.SetNodePressure(*server_entities.begin());

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;

    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.sceneConfId = scene_config_id0;

    std::unordered_map<entt::entity, entt::entity> player_scene1;

    EnterSceneParam enter_param1;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = nssys.FindSceneWithMinPlayerCount(weight_round_robin_scene);
        auto p_e = tls.game_node_registry.create();
        enter_param1.enter = p_e;
        enter_param1.scene = can_enter;
        player_scene1.emplace(enter_param1.enter, enter_param1.scene);
        sm.EnterScene(enter_param1);
    }

    uint32_t player_scene_guid = 0;

    nssys.ClearNodePressure(*server_entities.begin());

    std::unordered_map<entt::entity, entt::entity> player_scene2;
    weight_round_robin_scene.sceneConfId = scene_config_id1;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = nssys.FindSceneWithMinPlayerCount(weight_round_robin_scene);
        auto p_e = tls.game_node_registry.create();
        enter_param1.enter = p_e;
        enter_param1.scene = can_enter;
        player_scene2.emplace(enter_param1.enter, enter_param1.scene);
        sm.EnterScene(enter_param1);
    }
}

TEST(GS, EnterDefaultScene)
{
    const auto game_node = CreateMainSceneNode();
    CreateGameNodeSceneParam create_gs_scene_param{game_node};
    for (uint32_t i = 1; i < kConfigSceneListSize; ++i)
    {
        create_gs_scene_param.sceneInfo.set_scene_confid(i);
        for (uint32_t j = 0; j < kPerSceneConfigSize; ++j)
        {
            ScenesSystem::CreateScene2GameNode(create_gs_scene_param);
        }
    }
    const auto player = tls.game_node_registry.create();
    const EnterDefaultSceneParam enter_param{player};
    ScenesSystem::EnterDefaultScene(enter_param);
    const auto [sceneEntity] = tls.registry.get<SceneEntity>(player);
    const auto& scene_info = tls.scene_registry.get<SceneInfo>(sceneEntity);
    EXPECT_EQ(scene_info.scene_confid(), kDefaultSceneId);
}

struct TestNodeId
{
    uint32_t node_id_{ 0 };
};

TEST(GS, GetNotFullMainSceneWhenSceneFull)
{
	tls.game_node_registry.clear();
	ScenesSystem sm;
	NodeSceneSystem nssys;
	EntitySet server_entities;
	uint32_t server_size = 10;
	uint32_t per_server_scene = 10;

	for (uint32_t i = 0; i < server_size; ++i)
	{
        auto server = CreateMainSceneNode();
		server_entities.emplace(server);
        tls.game_node_registry.emplace<TestNodeId>(server).node_id_ = i;
	}

	CreateGameNodeSceneParam create_server_scene_param;

	for (uint32_t i = 0; i < per_server_scene; ++i)
	{
		create_server_scene_param.sceneInfo.set_scene_confid(i);
		for (auto& it : server_entities)
		{
			create_server_scene_param.node = it;
			auto s1 = sm.CreateScene2GameNode(create_server_scene_param);
            tls.game_node_registry.emplace<TestNodeId>(s1, tls.game_node_registry.get<TestNodeId>(it));
            auto s2 = sm.CreateScene2GameNode(create_server_scene_param);
            tls.game_node_registry.emplace<TestNodeId>(s2, tls.game_node_registry.get<TestNodeId>(it));    
		}
	}

	auto enter_leave_lambda = [&server_entities, server_size, per_server_scene, &sm, &nssys]()->void
	{
		uint32_t scene_config_id0 = 0;
		uint32_t scene_config_id1 = 1;
		GetSceneParam weight_round_robin_scene;
		weight_round_robin_scene.sceneConfId = scene_config_id0;

		uint32_t player_size = 1001;

		std::unordered_map<entt::entity, entt::entity> player_scene1;
		EnterSceneParam enter_param1;

		EntitySet scene_sets;

		for (uint32_t i = 0; i < player_size; ++i)
		{
			auto can_enter = nssys.FindNotFullScene(weight_round_robin_scene);
            if (can_enter == entt::null)
            {
                continue;
            }
			auto p_e = tls.game_node_registry.create();
			enter_param1.enter = p_e;
			enter_param1.scene = can_enter;
			player_scene1.emplace(enter_param1.enter, can_enter);
			scene_sets.emplace(can_enter);
			sm.EnterScene(enter_param1);
		}

		uint32_t player_scene_guid = 0;
		for (auto& it : player_scene1)
		{
			auto& pse = tls.registry.get<SceneEntity>(it.first);
			EXPECT_TRUE(pse.sceneEntity == it.second);
			EXPECT_EQ(tls.scene_registry.get<SceneInfo>(pse.sceneEntity).scene_confid(), scene_config_id0);
		}

		std::unordered_map<entt::entity, entt::entity> player_scene2;
		weight_round_robin_scene.sceneConfId = scene_config_id1;
		for (uint32_t i = 0; i < player_size; ++i)
		{
			auto can_enter = nssys.FindNotFullScene(weight_round_robin_scene);
			auto player = tls.game_node_registry.create();
			enter_param1.enter = player;
			enter_param1.scene = can_enter;
			player_scene2.emplace(enter_param1.enter, enter_param1.scene);
			scene_sets.emplace(can_enter);
			sm.EnterScene(enter_param1);
		}
		player_scene_guid = 0;
		for (auto& it : player_scene2)
		{
			auto& pse = tls.registry.get<SceneEntity>(it.first);
			EXPECT_TRUE(pse.sceneEntity == it.second);
			EXPECT_EQ(tls.scene_registry.get<SceneInfo>(pse.sceneEntity).scene_confid(), scene_config_id1);
		}

		std::size_t server_player_size = player_size * 2 / server_size;
        std::size_t remain_server_size = player_size * 2 - kMaxScenePlayerSize * 2;
		for (auto& it : server_entities)
		{
			auto& ps = tls.game_node_registry.get<GameNodeInfoPtr>(it);
            if (tls.game_node_registry.get<TestNodeId>(it).node_id_ == 9)
            {
                EXPECT_EQ((*ps).player_size(), kMaxServerPlayerSize);
            }
            else if (tls.game_node_registry.get<TestNodeId>(it).node_id_ == 8)
            {
                EXPECT_EQ((*ps).player_size(), remain_server_size);
            }
            else
            {
                EXPECT_EQ((*ps).player_size(), 0);
            }
		}
		EXPECT_EQ(scene_sets.size(), std::size_t(4));

		LeaveSceneParam leave_scene;
		for (auto& it : player_scene1)
		{
			auto& pse = tls.registry.get<SceneEntity>(it.first);
			leave_scene.leaver = it.first;
			sm.LeaveScene(leave_scene);
		}
		for (auto& it : player_scene2)
		{
			auto& pse = tls.registry.get<SceneEntity>(it.first);
			leave_scene.leaver = it.first;
			sm.LeaveScene(leave_scene);
		}
		for (auto& it : server_entities)
		{
			auto& ps = tls.game_node_registry.get<GameNodeInfoPtr>(it);
			EXPECT_EQ((*ps).player_size(), 0);
		}
		for (auto& it : player_scene1)
		{
			EXPECT_EQ(tls.scene_registry.get<ScenePlayers>(it.second).size(), 0);
		}
		for (auto& it : player_scene2)
		{
			EXPECT_EQ(tls.scene_registry.get<ScenePlayers>(it.second).size(), 0);
		}
	};
	for (uint32_t i = 0; i < 2; ++i)
	{
		enter_leave_lambda();
	}
}

TEST(GS, CreateDungeon)
{
}

TEST(GS, Route)
{
    tls.game_node_registry.clear();
}

TEST(GS, CheckEnterRoomScene)
{
    SceneInfo scene_info;
    for (uint64_t i = 1; i < 10; ++i)
    {
        scene_info.mutable_creators()->emplace(i,false);
    }
    auto scene = ScenesSystem::CreateScene2GameNode({.node= CreateMainSceneNode(), .sceneInfo = scene_info});

    const auto player = tls.registry.create();
    tls.registry.emplace<Guid>(player, 1);
    const auto player1 = tls.registry.create();
    tls.registry.emplace<Guid>(player1, 100);

    EXPECT_EQ(kOK, ScenesSystem::CheckPlayerEnterScene({.scene = scene, .enter = player}));
    EXPECT_EQ(kRetCheckEnterSceneCreator, ScenesSystem::CheckPlayerEnterScene({.scene = scene, .enter = player1}));
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
