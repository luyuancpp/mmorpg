#include <gtest/gtest.h>

#include "src/common_type/common_type.h"
#include "src/game_logic/scene/scene_system.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/thread_local/thread_local_storage.h"

#include "component_proto/gs_node_comp.pb.h"
#include "src/pb/pbc/component_proto/scene_comp.pb.h"

using GsNodePlayerInfoPtr = std::shared_ptr<GsNodePlayerInfo>;

std::size_t confid_scenelist_size = 50;
std::size_t per_scene_config_size = 2;

entt::entity CreateMainSceneNode()
{
    const auto scene_entity = tls.registry.create();
    AddMainSceneNodeComponent(scene_entity);
    return scene_entity;
}

TEST(GS, CreateMainScene)
{
    ScenesSystem sm;
    
    CreateGsSceneParam create_gs_scene_param;
    const auto server_entity1 = CreateMainSceneNode();
   
    create_gs_scene_param.node_ = server_entity1;
    for (uint32_t i = 0; i < confid_scenelist_size; ++i)
    {
        create_gs_scene_param.scene_confid_ = i;
        for (uint32_t j = 0; j < per_scene_config_size; ++j)
        {
            sm.CreateScene2Gs(create_gs_scene_param);
        }
        EXPECT_EQ(sm.scenes_size(i), per_scene_config_size);
    }
    EXPECT_EQ(sm.scenes_size(), confid_scenelist_size * per_scene_config_size);
    EXPECT_EQ(sm.scenes_size(), confid_scenelist_size * per_scene_config_size);
}

TEST(GS, CreateScene2Sever)
{
    const ScenesSystem sm;
    const auto server_entity1 = CreateMainSceneNode();
    const auto server_entity2 = CreateMainSceneNode();

    CreateGsSceneParam create_gs_scene_param1;
    CreateGsSceneParam create_gs_scene_param2;

    create_gs_scene_param1.scene_confid_ = 2;
    create_gs_scene_param1.node_ = server_entity1;

    create_gs_scene_param2.scene_confid_ = 3;
    create_gs_scene_param2.node_ = server_entity2;

    sm.CreateScene2Gs(create_gs_scene_param1);
    sm.CreateScene2Gs(create_gs_scene_param2);

    auto servercomp1 = tls.registry.try_get<ServerComp>(server_entity1);
    if (nullptr != servercomp1)
    {
        EXPECT_EQ(1, servercomp1->GetSceneSize());
    }
 
    auto servercomp2 = tls.registry.try_get<ServerComp>(server_entity2);
    if (nullptr != servercomp2)
    {
		EXPECT_EQ(1, servercomp2->GetSceneSize());
    }
    
    EXPECT_EQ(1, sm.scenes_size(create_gs_scene_param1.scene_confid_));

    EXPECT_EQ(1, sm.scenes_size(create_gs_scene_param2.scene_confid_));
    EXPECT_EQ(2, sm.scenes_size());
    EXPECT_EQ(sm.scenes_size(), sm.scenes_size());
}

TEST(GS, DestroyScene)
{
    ScenesSystem  sm;

    const auto server_entity1 = CreateMainSceneNode();

    CreateGsSceneParam create_gs_scene_param1;
    create_gs_scene_param1.node_ = server_entity1;
    const auto scene_entity = sm.CreateScene2Gs(create_gs_scene_param1);

    EXPECT_EQ(1, sm.scenes_size());
    EXPECT_EQ(1, sm.scenes_size(create_gs_scene_param1.scene_confid_));
    EXPECT_EQ(sm.scenes_size(), sm.scenes_size());

	auto servercomp1 = tls.registry.try_get<ServerComp>(server_entity1);
	if (nullptr != servercomp1)
	{
		EXPECT_EQ(1, servercomp1->GetSceneSize());
	}

    sm.DestroyScene(server_entity1, scene_entity);
    EXPECT_TRUE(sm.IsSceneEmpty());
    EXPECT_FALSE(sm.ConfigSceneListNotEmpty(create_gs_scene_param1.scene_confid_));
    EXPECT_TRUE(cl_tls.scene_list().empty());
    EXPECT_EQ(sm.scenes_size(), sm.scenes_size());
    EXPECT_FALSE(tls.registry.valid(scene_entity));
}

TEST(GS, DestroySever)
{
    ScenesSystem sm;

    auto server_entity1 = CreateMainSceneNode();
    auto server_entity2 = CreateMainSceneNode();

    CreateGsSceneParam create_gs_scene_param1;
    CreateGsSceneParam create_gs_scene_param2;
    create_gs_scene_param1.scene_confid_ = 3;
    create_gs_scene_param1.node_ = server_entity1;

    create_gs_scene_param2.scene_confid_ = 2;
    create_gs_scene_param2.node_ = server_entity2;

    auto scene_entity1 = sm.CreateScene2Gs(create_gs_scene_param1);
    auto scene_entity2 = sm.CreateScene2Gs(create_gs_scene_param2);

    EXPECT_EQ(1, tls.registry.get<ServerComp>(server_entity1).GetSceneSize());
    EXPECT_EQ(1, tls.registry.get<ServerComp>(server_entity2).GetSceneSize());

    EXPECT_EQ(2, sm.scenes_size());
    EXPECT_EQ(sm.scenes_size(), sm.scenes_size());

    sm.OnDestroyServer(server_entity1);

    EXPECT_FALSE(tls.registry.valid(server_entity1));
    EXPECT_FALSE(tls.registry.valid(scene_entity1));
    EXPECT_TRUE(tls.registry.valid(server_entity2));
    EXPECT_TRUE(tls.registry.valid(scene_entity2));

    EXPECT_EQ(1, tls.registry.get<ServerComp>(server_entity2).GetSceneSize());
    EXPECT_EQ(1, sm.scenes_size());
    EXPECT_EQ(0, sm.scenes_size(create_gs_scene_param1.scene_confid_));
    EXPECT_EQ(1, sm.scenes_size(create_gs_scene_param2.scene_confid_));

    sm.OnDestroyServer(server_entity2);

    EXPECT_EQ(0, sm.scenes_size());
    EXPECT_FALSE(tls.registry.valid(server_entity1));
    EXPECT_FALSE(tls.registry.valid(scene_entity1));
    EXPECT_FALSE(tls.registry.valid(server_entity2));
    EXPECT_FALSE(tls.registry.valid(scene_entity2));

    EXPECT_EQ(0, sm.scenes_size(create_gs_scene_param1.scene_confid_));
    EXPECT_EQ(0, sm.scenes_size(create_gs_scene_param2.scene_confid_));
    EXPECT_EQ(sm.scenes_size(), sm.scenes_size());
}

TEST(GS, PlayerLeaveEnterScene)
{
    const ScenesSystem sm;

    auto server_entity1 = CreateMainSceneNode();
    auto server_entity2 = CreateMainSceneNode();

    CreateGsSceneParam create_gs_scene_param1;
    CreateGsSceneParam create_gs_scene_param2;

    create_gs_scene_param1.scene_confid_ = 3;
    create_gs_scene_param1.node_ = server_entity1;

    create_gs_scene_param2.scene_confid_ = 2;
    create_gs_scene_param2.node_ = server_entity2;

    auto scene_guid1 = sm.CreateScene2Gs(create_gs_scene_param1);
    auto scene_guid2 = sm.CreateScene2Gs(create_gs_scene_param2);

    EnterSceneParam enter_param1;
    enter_param1.scene_ = scene_guid1;

    EnterSceneParam enter_param2;
    enter_param2.scene_ = scene_guid2;

    uint32_t player_size = 100;
    EntitySet player_entity_set1;
    EntitySet player_entities_set2;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto player_entity = tls.registry.create();

        if (i % 2 == 0)
        {
            player_entity_set1.emplace(player_entity);
            enter_param1.enterer_ = player_entity;
            sm.EnterScene(enter_param1);
        }
        else
        {
            player_entities_set2.emplace(player_entity);
            enter_param2.enterer_ = player_entity;
            sm.EnterScene(enter_param2);
        }
    }

    const auto& scenes_players1 = tls.registry.get<ScenePlayers>(scene_guid1);
    const auto& scenes_players2 = tls.registry.get<ScenePlayers>(scene_guid2);
    for (const auto& player_entity : player_entity_set1)
    {
        EXPECT_TRUE(scenes_players1.find(player_entity) != scenes_players1.end());
        EXPECT_TRUE(tls.registry.get<SceneEntity>(player_entity).scene_entity_ == scene_guid1);
    }
    for (const auto& player_entity : player_entities_set2)
    {
        EXPECT_TRUE(scenes_players2.find(player_entity) != scenes_players2.end());
        EXPECT_TRUE(tls.registry.get<SceneEntity>(player_entity).scene_entity_ == scene_guid2);
    }
    EXPECT_EQ(tls.registry.get<GsNodePlayerInfoPtr>(server_entity1)->player_size(), player_size / 2);
    EXPECT_EQ(tls.registry.get<GsNodePlayerInfoPtr>(server_entity2)->player_size(), player_size / 2);
    LeaveSceneParam leave_param1;
    for (const auto& player_entity : player_entity_set1)
    {
        leave_param1.leaver_ = player_entity;
        sm.LeaveScene(leave_param1);
        EXPECT_FALSE(scenes_players1.find(player_entity) != scenes_players1.end());
        EXPECT_EQ(tls.registry.try_get<SceneEntity>(player_entity), nullptr);
    }
    EXPECT_EQ(tls.registry.get<GsNodePlayerInfoPtr>(server_entity1)->player_size(), 0);

    LeaveSceneParam leave_param2;
    for (const auto& player_entity : player_entities_set2)
    {
        leave_param2.leaver_ = player_entity;
        sm.LeaveScene(leave_param2);
        EXPECT_FALSE(scenes_players2.find(player_entity) != scenes_players2.end());
        EXPECT_EQ(tls.registry.try_get<SceneEntity>(player_entity), nullptr);
    }

    EXPECT_EQ(tls.registry.get<GsNodePlayerInfoPtr>(server_entity2)->player_size(), 0);
    auto& scenes_players11 = tls.registry.get<ScenePlayers>(scene_guid1);
    auto& scenes_players22 = tls.registry.get<ScenePlayers>(scene_guid2);
    EXPECT_TRUE(scenes_players11.empty());
    EXPECT_TRUE(scenes_players22.empty());
}

TEST(GS, MainTainWeightRoundRobinMainScene)
{
    tls.registry.clear();
    ScenesSystem sm;
    ServerNodeSystem node_system;
    EntitySet server_entities;
    const uint32_t server_size = 2;
    const uint32_t per_server_scene = 2;
    EntitySet scene_entities;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        server_entities.emplace(CreateMainSceneNode());
    }

    CreateGsSceneParam create_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        create_server_scene_param.scene_confid_ = i;
        for (auto& it : server_entities)
        {
            create_server_scene_param.node_ = it;
            auto scene_entity = sm.CreateScene2Gs(create_server_scene_param);
            if (scene_entities.empty())
            {
                scene_entities.emplace(scene_entity);
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
            enter_param1.enterer_ = tls.registry.create();
            enter_param1.scene_ = scene_entity;
            player_scene1.emplace(enter_param1.enterer_, enter_param1.scene_);
            sm.EnterScene(enter_param1);
        }
    }
    ServerNodeSystem::SetNodeState(*server_entities.begin(), NodeState::kMaintain);

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

    auto server_entity1 = CreateMainSceneNode();
    auto server_entity2 = CreateMainSceneNode();

    CreateGsSceneParam server1_param;
    CreateGsSceneParam server2_param;

    server1_param.scene_confid_ = 2;
    server1_param.node_ = server_entity1;

    server2_param.scene_confid_ = 2;
    server2_param.node_ = server_entity2;

    const auto scene_guid1 = sm.CreateScene2Gs(server1_param);
    const auto scene_guid2 = sm.CreateScene2Gs(server2_param);

    EnterSceneParam enter_param1;
    enter_param1.scene_ = scene_guid1;

    EnterSceneParam enter_param2;
    enter_param2.scene_ = scene_guid2;

    constexpr uint32_t player_size = 100;
    EntitySet player_entities_set1;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto pe = tls.registry.create();
        player_entities_set1.emplace(pe);
        enter_param1.enterer_ = pe;
        sm.EnterScene(enter_param1);
    }

    CompelChangeSceneParam compel_change_param1;
    compel_change_param1.dest_node_ = server_entity2;
    compel_change_param1.scene_conf_id_ = server2_param.scene_confid_;
    for (auto& it : player_entities_set1)
    {
        compel_change_param1.player_ = it;
        sm.CompelPlayerChangeScene(compel_change_param1);
        EXPECT_TRUE(tls.registry.try_get<SceneEntity>(it)->scene_entity_ == scene_guid2);
    }
    EXPECT_EQ(tls.registry.get<GsNodePlayerInfoPtr>(server_entity1)->player_size(), 0);
    EXPECT_EQ(tls.registry.get<GsNodePlayerInfoPtr>(server_entity2)->player_size(), player_entities_set1.size());
    auto& scenes_players11 = tls.registry.get<ScenePlayers>(scene_guid1);
    auto& scenes_players22 = tls.registry.get<ScenePlayers>(scene_guid2);
    EXPECT_TRUE(scenes_players11.empty());
    EXPECT_EQ(scenes_players22.size(), player_entities_set1.size());
}


TEST(GS, CrashWeightRoundRobinMainScene)
{
    ScenesSystem sm;
    ServerNodeSystem snsys;
    EntitySet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 2;

    EntitySet scene_entities;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        server_entities.emplace(CreateMainSceneNode( ));
    }

    CreateGsSceneParam create_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        create_server_scene_param.scene_confid_ = i;
        for (auto& it : server_entities)
        {
            create_server_scene_param.node_ = it;
            auto e = sm.CreateScene2Gs(create_server_scene_param);
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
            enter_param1.enterer_ = p_e;
            enter_param1.scene_ = it;
            player_scene1.emplace(enter_param1.enterer_, enter_param1.scene_);
            sm.EnterScene(enter_param1);
        }
    }


    snsys.SetNodeState(*server_entities.begin(), NodeState::kCrash);

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;
    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_conf_id_ = scene_config_id0;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = snsys.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
        EXPECT_TRUE(can_enter != entt::null);
    }

}

//崩溃时候的消息不能处理
TEST(GS, CrashMovePlayer2NewServer)
{
    ScenesSystem sm;
    ServerNodeSystem snsys;
    EntitySet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 2;
    EntitySet scene_entities;
    entt::entity first_scene_guid = entt::null;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        server_entities.emplace(CreateMainSceneNode());
    }

    CreateGsSceneParam create_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        create_server_scene_param.scene_confid_ = i;
        for (auto& it : server_entities)
        {
            create_server_scene_param.node_ = it;
            auto e = sm.CreateScene2Gs(create_server_scene_param);
            scene_entities.emplace(e);
            if (first_scene_guid == entt::null)
            {
                first_scene_guid = e;
            }
        }
    }

    uint32_t player_size = 1000;

    std::unordered_map<entt::entity, entt::entity> player_scene1;

    EnterSceneParam enter_param1;

    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto p_e = tls.registry.create();
        enter_param1.enterer_ = p_e;
        enter_param1.scene_ = first_scene_guid;
        player_scene1.emplace(enter_param1.enterer_, enter_param1.scene_);
        sm.EnterScene(enter_param1);
    }
    
    snsys.SetNodeState(*server_entities.begin(), NodeState::kCrash);

   
    entt::entity cransh_node = *server_entities.begin();
     entt::entity replace_node = *(++server_entities.begin());
    sm.ReplaceCrashServer(cransh_node, replace_node);

    EXPECT_FALSE(tls.registry.valid(cransh_node));
    server_entities.erase(cransh_node);
    for (auto& it : server_entities)
    {
        auto& server_scene =  tls.registry.get<ServerComp>(it);
        EXPECT_EQ(server_scene.GetSceneSize(), scene_entities.size());
    }
    
}

TEST(GS, WeightRoundRobinMainScene)
{
    tls.registry.clear();
    ScenesSystem sm;
    ServerNodeSystem snsys;
    EntitySet server_entities;
    uint32_t server_size = 10;
    uint32_t per_server_scene = 10;
    for (uint32_t i = 0; i < server_size; ++i)
    {

        server_entities.emplace(CreateMainSceneNode());
    }

    CreateGsSceneParam create_server_scene_param;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        create_server_scene_param.scene_confid_ = i;
        for (auto& it :server_entities)
        {
            create_server_scene_param.node_ = it;
            sm.CreateScene2Gs(create_server_scene_param);
        }        
    }

    auto enter_leave_lambda = [&server_entities, server_size, per_server_scene, &sm, &snsys]()->void
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
            auto can_enter = snsys.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
            auto p_e = tls.registry.create();
            enter_param1.enterer_ = p_e;
            enter_param1.scene_ = can_enter;
            player_scene1.emplace(enter_param1.enterer_, can_enter);
            scene_sets.emplace(can_enter);
            sm.EnterScene(enter_param1);
        }

        uint32_t player_scene_guid = 0;
        for (auto& it : player_scene1)
        {
            auto& pse = tls.registry.get<SceneEntity>(it.first);
            EXPECT_TRUE(pse.scene_entity_ == it.second);
            EXPECT_EQ(tls.registry.get<SceneInfo>(pse.scene_entity_).scene_confid(), scene_config_id0);
        }

        std::unordered_map<entt::entity, entt::entity> player_scene2;
        weight_round_robin_scene.scene_conf_id_ = scene_config_id1;
        for (uint32_t i = 0; i < player_size; ++i)
        {
            auto can_enter = snsys.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
            auto p_e = tls.registry.create();
            enter_param1.enterer_ = p_e;
            enter_param1.scene_ = can_enter;
            player_scene2.emplace(enter_param1.enterer_, enter_param1.scene_);
            scene_sets.emplace(can_enter);
            sm.EnterScene(enter_param1);
        }
        player_scene_guid = 0;
        for (auto& it : player_scene2)
        {
            auto& pse = tls.registry.get<SceneEntity>(it.first);
            EXPECT_TRUE(pse.scene_entity_ == it.second);
            EXPECT_EQ(tls.registry.get<SceneInfo>(pse.scene_entity_).scene_confid(), scene_config_id1);
        }

        std::size_t server_player_size = player_size * 2 / server_size;


        for (auto& it : server_entities)
        {
            auto& ps = tls.registry.get<GsNodePlayerInfoPtr>(it);
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
        for (auto& it : server_entities)
        {
            auto& ps = tls.registry.get<GsNodePlayerInfoPtr>(it);
            EXPECT_EQ((*ps).player_size(), 0);
        }
        for (auto& it : player_scene1)
        {
            EXPECT_EQ(tls.registry.get<ScenePlayers>(it.second).size(), 0);
        }
        for (auto& it : player_scene2)
        {
            EXPECT_EQ(tls.registry.get<ScenePlayers>(it.second).size(), 0);
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
    ServerNodeSystem snsys;
    EntitySet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 10;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        server_entities.emplace(CreateMainSceneNode());
    }

    CreateGsSceneParam create_server_scene_param;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        create_server_scene_param.scene_confid_ = i;
        for (auto& it : server_entities)
        {
            create_server_scene_param.node_ = it;
            sm.CreateScene2Gs(create_server_scene_param);
        }
    }

    snsys.NodeEnterPressure(*server_entities.begin());

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;

    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_conf_id_ = scene_config_id0;

    std::unordered_map<entt::entity, entt::entity> player_scene1;

    EnterSceneParam enter_param1;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = snsys.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
        auto p_e = tls.registry.create();
        enter_param1.enterer_ = p_e;
        enter_param1.scene_ = can_enter;
        player_scene1.emplace(enter_param1.enterer_, enter_param1.scene_);
        sm.EnterScene(enter_param1);
    }

    uint32_t player_scene_guid = 0;

    snsys.NodeEnterNoPressure(*server_entities.begin());

    std::unordered_map<entt::entity, entt::entity> player_scene2;
    weight_round_robin_scene.scene_conf_id_ = scene_config_id1;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = snsys.GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
        auto p_e = tls.registry.create();
        enter_param1.enterer_ = p_e;
        enter_param1.scene_ = can_enter;
        player_scene2.emplace(enter_param1.enterer_, enter_param1.scene_);
        sm.EnterScene(enter_param1);
    }
}

struct TestNodeId
{
    uint32_t node_id_{ 0 };
};

TEST(GS, GetNotFullMainSceneSceneFull)
{
	tls.registry.clear();
	ScenesSystem sm;
	ServerNodeSystem snsys;
	EntitySet server_entities;
	uint32_t server_size = 10;
	uint32_t per_server_scene = 10;

	for (uint32_t i = 0; i < server_size; ++i)
	{
        auto server = CreateMainSceneNode();
		server_entities.emplace(server);
        tls.registry.emplace<TestNodeId>(server).node_id_ = i;
	}

	CreateGsSceneParam create_server_scene_param;

	for (uint32_t i = 0; i < per_server_scene; ++i)
	{
		create_server_scene_param.scene_confid_ = i;
		for (auto& it : server_entities)
		{
			create_server_scene_param.node_ = it;
			auto s1 = sm.CreateScene2Gs(create_server_scene_param);
            tls.registry.emplace<TestNodeId>(s1, tls.registry.get<TestNodeId>(it));
            auto s2 = sm.CreateScene2Gs(create_server_scene_param);
            tls.registry.emplace<TestNodeId>(s2, tls.registry.get<TestNodeId>(it));    
		}
	}

	auto enter_leave_lambda = [&server_entities, server_size, per_server_scene, &sm, &snsys]()->void
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
			auto can_enter = snsys.GetNotFullScene(weight_round_robin_scene);
            if (can_enter == entt::null)
            {
                continue;
            }
			auto p_e = tls.registry.create();
			enter_param1.enterer_ = p_e;
			enter_param1.scene_ = can_enter;
			player_scene1.emplace(enter_param1.enterer_, can_enter);
			scene_sets.emplace(can_enter);
			sm.EnterScene(enter_param1);
		}

		uint32_t player_scene_guid = 0;
		for (auto& it : player_scene1)
		{
			auto& pse = tls.registry.get<SceneEntity>(it.first);
			EXPECT_TRUE(pse.scene_entity_ == it.second);
			EXPECT_EQ(tls.registry.get<SceneInfo>(pse.scene_entity_).scene_confid(), scene_config_id0);
		}

		std::unordered_map<entt::entity, entt::entity> player_scene2;
		weight_round_robin_scene.scene_conf_id_ = scene_config_id1;
		for (uint32_t i = 0; i < player_size; ++i)
		{
			auto can_enter = snsys.GetNotFullScene(weight_round_robin_scene);
			auto p_e = tls.registry.create();
			enter_param1.enterer_ = p_e;
			enter_param1.scene_ = can_enter;
			player_scene2.emplace(enter_param1.enterer_, enter_param1.scene_);
			scene_sets.emplace(can_enter);
			sm.EnterScene(enter_param1);
		}
		player_scene_guid = 0;
		for (auto& it : player_scene2)
		{
			auto& pse = tls.registry.get<SceneEntity>(it.first);
			EXPECT_TRUE(pse.scene_entity_ == it.second);
			EXPECT_EQ(tls.registry.get<SceneInfo>(pse.scene_entity_).scene_confid(), scene_config_id1);
		}

		std::size_t server_player_size = player_size * 2 / server_size;
        std::size_t remain_server_size = player_size * 2 - kMaxScenePlayerSize * 2;
		for (auto& it : server_entities)
		{
			auto& ps = tls.registry.get<GsNodePlayerInfoPtr>(it);
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
			auto& ps = tls.registry.get<GsNodePlayerInfoPtr>(it);
			EXPECT_EQ((*ps).player_size(), 0);
		}
		for (auto& it : player_scene1)
		{
			EXPECT_EQ(tls.registry.get<ScenePlayers>(it.second).size(), 0);
		}
		for (auto& it : player_scene2)
		{
			EXPECT_EQ(tls.registry.get<ScenePlayers>(it.second).size(), 0);
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
