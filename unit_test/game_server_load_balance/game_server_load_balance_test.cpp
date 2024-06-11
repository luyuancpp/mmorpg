#include <gtest/gtest.h>

#include "src/system/scene/scene_system.h"
#include "src/comp/scene_comp.h"
#include "src/thread_local/thread_local_storage.h"

#include "src/component_proto/gs_node_comp.pb.h"
#include "src/component_proto/scene_comp.pb.h"

using GameNodeInfoPtr = std::shared_ptr<GameNodeInfo>;

std::size_t kConfigSceneListSize = 50;
std::size_t kPerSceneConfigSize = 2;

entt::entity CreateMainSceneNode()
{
    const auto node = tls.registry.create();
    AddMainSceneNodeComponent(node);
    return node;
}

TEST(GS, CreateMainScene)
{
    const ScenesSystem sm;

    CreateGameNodeSceneParam create_gs_scene_param;
    const auto server_entity1 = CreateMainSceneNode();

    create_gs_scene_param.node_ = server_entity1;
    for (uint32_t i = 0; i < kConfigSceneListSize; ++i)
    {
        create_gs_scene_param.scene_info.set_scene_confid(i);
        for (uint32_t j = 0; j < kPerSceneConfigSize; ++j)
        {
            sm.CreateScene2GameNode(create_gs_scene_param);
        }
        EXPECT_EQ(sm.scenes_size(i), kPerSceneConfigSize);
    }
    EXPECT_EQ(sm.scenes_size(), kConfigSceneListSize * kPerSceneConfigSize);
    EXPECT_EQ(sm.scenes_size(), kConfigSceneListSize * kPerSceneConfigSize);
}

TEST(GS, CreateScene2Sever)
{
    const ScenesSystem sm;
    const auto node1 = CreateMainSceneNode();
    const auto node2 = CreateMainSceneNode();

    CreateGameNodeSceneParam create_gs_scene_param1;
    CreateGameNodeSceneParam create_gs_scene_param2;

    create_gs_scene_param1.scene_info.set_scene_confid(2);
    create_gs_scene_param1.node_ = node1;

    create_gs_scene_param2.scene_info.set_scene_confid(3);
    create_gs_scene_param2.node_ = node2;

    sm.CreateScene2GameNode(create_gs_scene_param1);
    sm.CreateScene2GameNode(create_gs_scene_param2);

    const auto nodecomp1 = tls.registry.try_get<ServerComp>(node1);
    if (nullptr != nodecomp1)
    {
        EXPECT_EQ(1, nodecomp1->GetSceneSize());
    }

    const auto nodecomp2 = tls.registry.try_get<ServerComp>(node2);
    if (nullptr != nodecomp2)
    {
		EXPECT_EQ(1, nodecomp2->GetSceneSize());
    }

    EXPECT_EQ(1, sm.scenes_size(create_gs_scene_param1.scene_info.scene_confid()));

    EXPECT_EQ(1, sm.scenes_size(create_gs_scene_param2.scene_info.scene_confid()));
    EXPECT_EQ(2, sm.scenes_size());
    EXPECT_EQ(sm.scenes_size(), sm.scenes_size());
}

TEST(GS, DestroyScene)
{
    const ScenesSystem sm;

    const auto server_entity1 = CreateMainSceneNode();

    CreateGameNodeSceneParam create_gs_scene_param1;
    create_gs_scene_param1.node_ = server_entity1;
    const auto scene_entity = sm.CreateScene2GameNode(create_gs_scene_param1);

    EXPECT_EQ(1, sm.scenes_size());
    EXPECT_EQ(1, sm.scenes_size(create_gs_scene_param1.scene_info.scene_confid()));
    EXPECT_EQ(sm.scenes_size(), sm.scenes_size());

	auto servercomp1 = tls.registry.try_get<ServerComp>(server_entity1);
	if (nullptr != servercomp1)
	{
		EXPECT_EQ(1, servercomp1->GetSceneSize());
	}

    sm.DestroyScene(server_entity1, scene_entity);
    EXPECT_TRUE(sm.IsSceneEmpty());
    EXPECT_FALSE(sm.ConfigSceneListNotEmpty(create_gs_scene_param1.scene_info.scene_confid()));
    EXPECT_TRUE(sm.IsSceneEmpty());
    EXPECT_EQ(sm.scenes_size(), sm.scenes_size());
    EXPECT_FALSE(tls.registry.valid(scene_entity));
}

TEST(GS, DestroySever)
{
    ScenesSystem sm;

    auto node1 = CreateMainSceneNode();
    auto node2 = CreateMainSceneNode();

    CreateGameNodeSceneParam create_gs_scene_param1;
    CreateGameNodeSceneParam create_gs_scene_param2;
    create_gs_scene_param1.scene_info.set_scene_confid( 3);
    create_gs_scene_param1.node_ = node1;

    create_gs_scene_param2.scene_info.set_scene_confid(2);
    create_gs_scene_param2.node_ = node2;

    auto scene1 = sm.CreateScene2GameNode(create_gs_scene_param1);
    auto scene2 = sm.CreateScene2GameNode(create_gs_scene_param2);

    EXPECT_EQ(1, tls.registry.get<ServerComp>(node1).GetSceneSize());
    EXPECT_EQ(1, tls.registry.get<ServerComp>(node2).GetSceneSize());

    EXPECT_EQ(2, sm.scenes_size());
    EXPECT_EQ(sm.scenes_size(), sm.scenes_size());

    sm.OnDestroyServer(node1);

    EXPECT_FALSE(tls.registry.valid(node1));
    EXPECT_FALSE(tls.scene_registry.valid(scene1));
    EXPECT_TRUE(tls.registry.valid(node2));
    EXPECT_TRUE(tls.scene_registry.valid(scene2));

    EXPECT_EQ(1, tls.registry.get<ServerComp>(node2).GetSceneSize());
    EXPECT_EQ(1, sm.scenes_size());
    EXPECT_EQ(0, sm.scenes_size(create_gs_scene_param1.scene_info.scene_confid()));
    EXPECT_EQ(1, sm.scenes_size(create_gs_scene_param2.scene_info.scene_confid()));

    sm.OnDestroyServer(node2);

    EXPECT_EQ(0, sm.scenes_size());
    EXPECT_FALSE(tls.registry.valid(node1));
    EXPECT_FALSE(tls.scene_registry.valid(scene1));
    EXPECT_FALSE(tls.registry.valid(node2));
    EXPECT_FALSE(tls.scene_registry.valid(scene2));

    EXPECT_EQ(0, sm.scenes_size(create_gs_scene_param1.scene_info.scene_confid()));
    EXPECT_EQ(0, sm.scenes_size(create_gs_scene_param2.scene_info.scene_confid()));
    EXPECT_EQ(sm.scenes_size(), sm.scenes_size());
}

TEST(GS, PlayerLeaveEnterScene)
{
    const ScenesSystem sm;

    auto node1 = CreateMainSceneNode();
    auto node2 = CreateMainSceneNode();

    CreateGameNodeSceneParam create_gs_scene_param1;
    CreateGameNodeSceneParam create_gs_scene_param2;

    create_gs_scene_param1.scene_info.set_scene_confid(3);
    create_gs_scene_param1.node_ = node1;

    create_gs_scene_param2.scene_info.set_scene_confid(2);
    create_gs_scene_param2.node_ = node2;

    auto scene1 = sm.CreateScene2GameNode(create_gs_scene_param1);
    auto scene2 = sm.CreateScene2GameNode(create_gs_scene_param2);

    EnterSceneParam enter_param1;
    enter_param1.scene_ = scene1;

    EnterSceneParam enter_param2;
    enter_param2.scene_ = scene2;

    uint32_t player_size = 100;
    EntitySet player_entity_set1;
    EntitySet player_entities_set2;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto player_entity = tls.registry.create();

        if (i % 2 == 0)
        {
            player_entity_set1.emplace(player_entity);
            enter_param1.player_ = player_entity;
            sm.EnterScene(enter_param1);
        }
        else
        {
            player_entities_set2.emplace(player_entity);
            enter_param2.player_ = player_entity;
            sm.EnterScene(enter_param2);
        }
    }

    const auto& scenes_players1 = tls.scene_registry.get<ScenePlayers>(scene1);
    const auto& scenes_players2 = tls.scene_registry.get<ScenePlayers>(scene2);
    for (const auto& player_entity : player_entity_set1)
    {
        EXPECT_TRUE(scenes_players1.find(player_entity) != scenes_players1.end());
        EXPECT_TRUE(tls.registry.get<SceneEntity>(player_entity).scene_entity_ == scene1);
    }
    for (const auto& player_entity : player_entities_set2)
    {
        EXPECT_TRUE(scenes_players2.find(player_entity) != scenes_players2.end());
        EXPECT_TRUE(tls.registry.get<SceneEntity>(player_entity).scene_entity_ == scene2);
    }
    EXPECT_EQ(tls.registry.get<GameNodeInfoPtr>(node1)->player_size(), player_size / 2);
    EXPECT_EQ(tls.registry.get<GameNodeInfoPtr>(node2)->player_size(), player_size / 2);
    LeaveSceneParam leave_param1;
    for (const auto& player_entity : player_entity_set1)
    {
        leave_param1.leaver_ = player_entity;
        sm.LeaveScene(leave_param1);
        EXPECT_FALSE(scenes_players1.find(player_entity) != scenes_players1.end());
        EXPECT_EQ(tls.registry.try_get<SceneEntity>(player_entity), nullptr);
    }
    EXPECT_EQ(tls.registry.get<GameNodeInfoPtr>(node1)->player_size(), 0);

    LeaveSceneParam leave_param2;
    for (const auto& player_entity : player_entities_set2)
    {
        leave_param2.leaver_ = player_entity;
        sm.LeaveScene(leave_param2);
        EXPECT_FALSE(scenes_players2.find(player_entity) != scenes_players2.end());
        EXPECT_EQ(tls.registry.try_get<SceneEntity>(player_entity), nullptr);
    }

    EXPECT_EQ(tls.registry.get<GameNodeInfoPtr>(node2)->player_size(), 0);
    auto& scenes_players11 = tls.scene_registry.get<ScenePlayers>(scene1);
    auto& scenes_players22 = tls.scene_registry.get<ScenePlayers>(scene2);
    EXPECT_TRUE(scenes_players11.empty());
    EXPECT_TRUE(scenes_players22.empty());
}

TEST(GS, MainTainWeightRoundRobinMainScene)
{
    tls.registry.clear();
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
        create_server_scene_param.scene_info.set_scene_confid(i);
        for (auto& it : server_entities)
        {
            create_server_scene_param.node_ = it;
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
        for (const auto scene_entity : scene_entities)
        {
            enter_param1.player_ = tls.registry.create();
            enter_param1.scene_ = scene_entity;
            player_scene1.emplace(enter_param1.player_, enter_param1.scene_);
            sm.EnterScene(enter_param1);
        }
    }
    NodeSceneSystem::SetNodeState(*server_entities.begin(), NodeState::kMaintain);

    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_conf_id_ = 0;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = node_system.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
        EXPECT_TRUE(can_enter != entt::null);
    }

    weight_round_robin_scene.scene_conf_id_ = 1;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = node_system.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
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

    server1_param.scene_info.set_scene_confid( 2);
    server1_param.node_ = node1;

    server2_param.scene_info.set_scene_confid( 2);
    server2_param.node_ = node2;

    const auto scene1 = sm.CreateScene2GameNode(server1_param);
    const auto scene2 = sm.CreateScene2GameNode(server2_param);

    EnterSceneParam enter_param1;
    enter_param1.scene_ = scene1;

    EnterSceneParam enter_param2;
    enter_param2.scene_ = scene2;

    constexpr uint32_t player_size = 100;
    EntitySet player_list1;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto player = tls.registry.create();
        player_list1.emplace(player);
        enter_param1.player_ = player;
        sm.EnterScene(enter_param1);
    }

    CompelChangeSceneParam compel_change_param1;
    compel_change_param1.dest_node_ = node2;
    compel_change_param1.scene_conf_id_ = server2_param.scene_info.scene_confid();
    for (auto& it : player_list1)
    {
        compel_change_param1.player_ = it;
        sm.CompelPlayerChangeScene(compel_change_param1);
        EXPECT_TRUE(tls.registry.try_get<SceneEntity>(it)->scene_entity_ == scene2);
    }
    EXPECT_EQ(tls.registry.get<GameNodeInfoPtr>(node1)->player_size(), 0);
    EXPECT_EQ(tls.registry.get<GameNodeInfoPtr>(node2)->player_size(), player_list1.size());
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
        create_server_scene_param.scene_info.set_scene_confid(i);
        for (auto& it : server_entities)
        {
            create_server_scene_param.node_ = it;
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
            auto p_e = tls.registry.create();
            enter_param1.player_ = p_e;
            enter_param1.scene_ = it;
            player_scene1.emplace(enter_param1.player_, enter_param1.scene_);
            sm.EnterScene(enter_param1);
        }
    }


    nssys.SetNodeState(*server_entities.begin(), NodeState::kCrash);

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;
    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_conf_id_ = scene_config_id0;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = nssys.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
        EXPECT_TRUE(can_enter != entt::null);
    }

}

//崩溃时候的消息不能处理
TEST(GS, CrashMovePlayer2NewServer)
{
    ScenesSystem sm;
    NodeSceneSystem nssys;
    EntitySet node_list;
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
        create_node_scene_param.scene_info.set_scene_confid(i);
        for (auto& it : node_list)
        {
            create_node_scene_param.node_ = it;
            auto e = sm.CreateScene2GameNode(create_node_scene_param);
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
        auto player = tls.registry.create();
        enter_param1.player_ = player;
        enter_param1.scene_ = first_scene;
        player_scene1.emplace(enter_param1.player_, enter_param1.scene_);
        sm.EnterScene(enter_param1);
    }
    
    nssys.SetNodeState(*node_list.begin(), NodeState::kCrash);

   
    entt::entity cransh_node = *node_list.begin();
     entt::entity replace_node = *(++node_list.begin());
    sm.ReplaceCrashServer(cransh_node, replace_node);

    EXPECT_FALSE(tls.registry.valid(cransh_node));
    node_list.erase(cransh_node);
    for (auto& it : node_list)
    {
        auto& server_scene =  tls.registry.get<ServerComp>(it);
        EXPECT_EQ(server_scene.GetSceneSize(), per_node_scene);
    }
    
}

TEST(GS, WeightRoundRobinMainScene)
{
    tls.registry.clear();
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
        create_server_scene_param.scene_info.set_scene_confid(i);
        for (auto& it :node_list)
        {
            create_server_scene_param.node_ = it;
            sm.CreateScene2GameNode(create_server_scene_param);
        }        
    }

    auto enter_leave_lambda = [&node_list, server_size, per_server_scene, &sm, &nssys]()->void
    {
        uint32_t scene_config_id0 = 0;
        uint32_t scene_config_id1 = 1;
        GetSceneParam weight_round_robin_scene;
        weight_round_robin_scene.scene_conf_id_ = scene_config_id0;

        uint32_t player_size = 1000;

        std::unordered_map<entt::entity, entt::entity> player_scene1;
        EnterSceneParam enter_param1;

        EntitySet scene_sets;

        for (uint32_t i = 0; i < player_size; ++i)
        {
            auto can_enter = nssys.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
            auto p_e = tls.registry.create();
            enter_param1.player_ = p_e;
            enter_param1.scene_ = can_enter;
            player_scene1.emplace(enter_param1.player_, can_enter);
            scene_sets.emplace(can_enter);
            sm.EnterScene(enter_param1);
        }

        uint32_t player_scene_guid = 0;
        for (auto& it : player_scene1)
        {
            auto& pse = tls.registry.get<SceneEntity>(it.first);
            EXPECT_TRUE(pse.scene_entity_ == it.second);
            EXPECT_EQ(tls.scene_registry.get<SceneInfo>(pse.scene_entity_).scene_confid(), scene_config_id0);
        }

        std::unordered_map<entt::entity, entt::entity> player_scene2;
        weight_round_robin_scene.scene_conf_id_ = scene_config_id1;
        for (uint32_t i = 0; i < player_size; ++i)
        {
            auto can_enter = nssys.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
            auto player = tls.registry.create();
            enter_param1.player_ = player;
            enter_param1.scene_ = can_enter;
            player_scene2.emplace(enter_param1.player_, enter_param1.scene_);
            scene_sets.emplace(can_enter);
            sm.EnterScene(enter_param1);
        }
        player_scene_guid = 0;
        for (auto& it : player_scene2)
        {
            auto& pse = tls.registry.get<SceneEntity>(it.first);
            EXPECT_TRUE(pse.scene_entity_ == it.second);
            EXPECT_EQ(tls.scene_registry.get<SceneInfo>(pse.scene_entity_).scene_confid(), scene_config_id1);
        }

        std::size_t server_player_size = player_size * 2 / server_size;


        for (auto& it : node_list)
        {
            auto& ps = tls.registry.get<GameNodeInfoPtr>(it);
            EXPECT_EQ((*ps).player_size(), server_player_size);
        }
        EXPECT_EQ(scene_sets.size(), std::size_t(2 * per_server_scene));

        LeaveSceneParam leave_scene;
        for (auto& it : player_scene1)
        {
            auto& pse = tls.registry.get<SceneEntity>(it.first);
            leave_scene.leaver_ = it.first;
            sm.LeaveScene(leave_scene);
        }
        for (auto& it : player_scene2)
        {
            auto& pse = tls.registry.get<SceneEntity>(it.first);
            leave_scene.leaver_ = it.first;
            sm.LeaveScene(leave_scene);
        }
        for (auto& it : node_list)
        {
            auto& ps = tls.registry.get<GameNodeInfoPtr>(it);
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
    tls.registry.clear();
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
        create_server_scene_param.scene_info.set_scene_confid(i);
        for (auto& it : server_entities)
        {
            create_server_scene_param.node_ = it;
            sm.CreateScene2GameNode(create_server_scene_param);
        }
    }

    nssys.NodeEnterPressure(*server_entities.begin());

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;

    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_conf_id_ = scene_config_id0;

    std::unordered_map<entt::entity, entt::entity> player_scene1;

    EnterSceneParam enter_param1;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = nssys.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
        auto p_e = tls.registry.create();
        enter_param1.player_ = p_e;
        enter_param1.scene_ = can_enter;
        player_scene1.emplace(enter_param1.player_, enter_param1.scene_);
        sm.EnterScene(enter_param1);
    }

    uint32_t player_scene_guid = 0;

    nssys.NodeEnterNoPressure(*server_entities.begin());

    std::unordered_map<entt::entity, entt::entity> player_scene2;
    weight_round_robin_scene.scene_conf_id_ = scene_config_id1;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = nssys.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
        auto p_e = tls.registry.create();
        enter_param1.player_ = p_e;
        enter_param1.scene_ = can_enter;
        player_scene2.emplace(enter_param1.player_, enter_param1.scene_);
        sm.EnterScene(enter_param1);
    }
}

TEST(GS, EnterDefaultScene)
{
    const auto game_node = CreateMainSceneNode();
    CreateGameNodeSceneParam create_gs_scene_param{game_node};
    for (uint32_t i = 1; i < kConfigSceneListSize; ++i)
    {
        create_gs_scene_param.scene_info.set_scene_confid(i);
        for (uint32_t j = 0; j < kPerSceneConfigSize; ++j)
        {
            ScenesSystem::CreateScene2GameNode(create_gs_scene_param);
        }
    }
    const auto player = tls.registry.create();
    const EnterDefaultSceneParam enter_param{player};
    ScenesSystem::EnterDefaultScene(enter_param);
    const auto [scene_entity_] = tls.registry.get<SceneEntity>(player);
    const auto& scene_info = tls.scene_registry.get<SceneInfo>(scene_entity_);
    EXPECT_EQ(scene_info.scene_confid(), kDefaultSceneId);
}

struct TestNodeId
{
    uint32_t node_id_{ 0 };
};

TEST(GS, GetNotFullMainSceneSceneFull)
{
	tls.registry.clear();
	ScenesSystem sm;
	NodeSceneSystem nssys;
	EntitySet server_entities;
	uint32_t server_size = 10;
	uint32_t per_server_scene = 10;

	for (uint32_t i = 0; i < server_size; ++i)
	{
        auto server = CreateMainSceneNode();
		server_entities.emplace(server);
        tls.registry.emplace<TestNodeId>(server).node_id_ = i;
	}

	CreateGameNodeSceneParam create_server_scene_param;

	for (uint32_t i = 0; i < per_server_scene; ++i)
	{
		create_server_scene_param.scene_info.set_scene_confid(i);
		for (auto& it : server_entities)
		{
			create_server_scene_param.node_ = it;
			auto s1 = sm.CreateScene2GameNode(create_server_scene_param);
            tls.registry.emplace<TestNodeId>(s1, tls.registry.get<TestNodeId>(it));
            auto s2 = sm.CreateScene2GameNode(create_server_scene_param);
            tls.registry.emplace<TestNodeId>(s2, tls.registry.get<TestNodeId>(it));    
		}
	}

	auto enter_leave_lambda = [&server_entities, server_size, per_server_scene, &sm, &nssys]()->void
	{
		uint32_t scene_config_id0 = 0;
		uint32_t scene_config_id1 = 1;
		GetSceneParam weight_round_robin_scene;
		weight_round_robin_scene.scene_conf_id_ = scene_config_id0;

		uint32_t player_size = 1001;

		std::unordered_map<entt::entity, entt::entity> player_scene1;
		EnterSceneParam enter_param1;

		EntitySet scene_sets;

		for (uint32_t i = 0; i < player_size; ++i)
		{
			auto can_enter = nssys.GetNotFullScene(weight_round_robin_scene);
            if (can_enter == entt::null)
            {
                continue;
            }
			auto p_e = tls.registry.create();
			enter_param1.player_ = p_e;
			enter_param1.scene_ = can_enter;
			player_scene1.emplace(enter_param1.player_, can_enter);
			scene_sets.emplace(can_enter);
			sm.EnterScene(enter_param1);
		}

		uint32_t player_scene_guid = 0;
		for (auto& it : player_scene1)
		{
			auto& pse = tls.registry.get<SceneEntity>(it.first);
			EXPECT_TRUE(pse.scene_entity_ == it.second);
			EXPECT_EQ(tls.scene_registry.get<SceneInfo>(pse.scene_entity_).scene_confid(), scene_config_id0);
		}

		std::unordered_map<entt::entity, entt::entity> player_scene2;
		weight_round_robin_scene.scene_conf_id_ = scene_config_id1;
		for (uint32_t i = 0; i < player_size; ++i)
		{
			auto can_enter = nssys.GetNotFullScene(weight_round_robin_scene);
			auto player = tls.registry.create();
			enter_param1.player_ = player;
			enter_param1.scene_ = can_enter;
			player_scene2.emplace(enter_param1.player_, enter_param1.scene_);
			scene_sets.emplace(can_enter);
			sm.EnterScene(enter_param1);
		}
		player_scene_guid = 0;
		for (auto& it : player_scene2)
		{
			auto& pse = tls.registry.get<SceneEntity>(it.first);
			EXPECT_TRUE(pse.scene_entity_ == it.second);
			EXPECT_EQ(tls.scene_registry.get<SceneInfo>(pse.scene_entity_).scene_confid(), scene_config_id1);
		}

		std::size_t server_player_size = player_size * 2 / server_size;
        std::size_t remain_server_size = player_size * 2 - kMaxScenePlayerSize * 2;
		for (auto& it : server_entities)
		{
			auto& ps = tls.registry.get<GameNodeInfoPtr>(it);
            if (tls.registry.get<TestNodeId>(it).node_id_ == 9)
            {
                EXPECT_EQ((*ps).player_size(), kMaxServerPlayerSize);
            }
            else if (tls.registry.get<TestNodeId>(it).node_id_ == 8)
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
			leave_scene.leaver_ = it.first;
			sm.LeaveScene(leave_scene);
		}
		for (auto& it : player_scene2)
		{
			auto& pse = tls.registry.get<SceneEntity>(it.first);
			leave_scene.leaver_ = it.first;
			sm.LeaveScene(leave_scene);
		}
		for (auto& it : server_entities)
		{
			auto& ps = tls.registry.get<GameNodeInfoPtr>(it);
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
    tls.registry.clear();
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
