#include <gtest/gtest.h>

#include "src/common_type/common_type.h"
#include "src/game_logic/scene/scene_factories.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/scene/scene.h"

#include "src/pb/pbc/component_proto/scene_comp.pb.h"

uint32_t confid_scenelist_size = 50;
uint32_t per_scene_config_size = 2;

TEST(GS, CreateMainScene)
{
    ScenesSystem sm;
    MakeSceneP param;
   
    for (uint32_t i = 0; i < confid_scenelist_size; ++i)
    {
        param.scene_confid_ = i;
        for (uint32_t j = 0; j < per_scene_config_size; ++j)
        {
            sm.MakeScene(param);
        }
        EXPECT_EQ(sm.scenes_size(i), std::size_t(per_scene_config_size));
    }
    EXPECT_EQ(sm.scenes_size(), std::size_t(confid_scenelist_size * per_scene_config_size));
    EXPECT_EQ(sm.scenes_size(), std::size_t(confid_scenelist_size * per_scene_config_size));
}

TEST(GS, MakeScene2Sever )
{
    ScenesSystem sm;

    MakeGSParam param1;
    param1.node_id_ = 1;

    auto server_entity1 = MakeMainSceneNode( param1);

    MakeGSParam param2;
    param2.node_id_ = 1;
    auto server_entity2 = MakeMainSceneNode( param2);

    MakeGsSceneP server1_param;
    MakeGsSceneP server2_param;

    server1_param.scene_confid_ = 2;
    server1_param.node_ = server_entity1;

    server2_param.scene_confid_ = 3;
    server2_param.node_ = server_entity2;

    sm.MakeScene2Gs(server1_param);
    sm.MakeScene2Gs(server2_param);

    auto& scenes_id1 = registry.get<ConfigSceneMap>(server_entity1);
 
    auto& scenes_id2 = registry.get<ConfigSceneMap>(server_entity2);

    EXPECT_EQ(1, scenes_id1.scenes_size());

    EXPECT_EQ(1, sm.scenes_size(server1_param.scene_confid_));

    EXPECT_EQ(1, scenes_id2.scenes_size());

    EXPECT_EQ(1, sm.scenes_size(server2_param.scene_confid_));
    EXPECT_EQ(2, sm.scenes_size());
    EXPECT_EQ(sm.scenes_size(), sm.scenes_map_size());
}

TEST(GS, DestroyScene)
{
    ScenesSystem sm;

    MakeGSParam param1;
    param1.node_id_ = 1;

    auto server_entity1 = MakeMainSceneNode( param1);

	MakeGsSceneP cparam;
    cparam.node_ = server_entity1;
	auto scene_entity = sm.MakeScene2Gs(cparam);
    
    EXPECT_EQ(1, sm.scenes_size());
    EXPECT_EQ(1, sm.scenes_size(cparam.scene_confid_));
    EXPECT_EQ(sm.scenes_size(), sm.scenes_map_size());

    auto& server_scenes = registry.get<ConfigSceneMap>(server_entity1);
    EXPECT_EQ(1, server_scenes.scenes_size());

    DestroySceneParam dparam;
    dparam.server_ = server_entity1;
    dparam.scene_ = scene_entity;
    sm.DestroyScene(dparam);
    EXPECT_TRUE(sm.Empty());
    EXPECT_FALSE(sm.HasScene(cparam.scene_confid_));
    EXPECT_TRUE(server_scenes.scenes_empty());
    EXPECT_EQ(sm.scenes_size(), sm.scenes_map_size());
    EXPECT_FALSE(registry.valid(scene_entity));
}

TEST(GS, DestroySever)
{
    ScenesSystem sm;

    MakeGSParam param1;
    param1.node_id_ = 1;

    auto server_entity1 = MakeMainSceneNode( param1);

    MakeGSParam param2;
    param2.node_id_ = 2;
    auto server_entity2 = MakeMainSceneNode( param2);

    MakeGsSceneP server1_param;
    MakeGsSceneP server2_param;
    server1_param.scene_confid_ = 3;
    server1_param.node_ = server_entity1;

    server2_param.scene_confid_ = 2;
    server2_param.node_ = server_entity2;

    auto scene_id1 = sm.MakeScene2Gs(server1_param);
    auto scene_id2 = sm.MakeScene2Gs(server2_param);

    EXPECT_EQ(1, registry.get<ConfigSceneMap>(server_entity1).scenes_size());
    EXPECT_EQ(1, registry.get<ConfigSceneMap>(server_entity2).scenes_size());

    EXPECT_EQ(2, sm.scenes_size());
    EXPECT_EQ(sm.scenes_size(), sm.scenes_map_size());

    DestroyServerParam destroy_server_param;
    destroy_server_param.server_ = server_entity1;
    sm.DestroyServer(destroy_server_param);

    EXPECT_FALSE(registry.valid(server_entity1));
    EXPECT_FALSE(registry.valid(scene_id1));
    EXPECT_TRUE(registry.valid(server_entity2));
    EXPECT_TRUE(registry.valid(scene_id2));

    EXPECT_EQ(1, registry.get<ConfigSceneMap>(server_entity2).scenes_size());
    EXPECT_EQ(1, sm.scenes_size());
    EXPECT_EQ(0, sm.scenes_size(server1_param.scene_confid_));
    EXPECT_EQ(1, sm.scenes_size(server2_param.scene_confid_));

    destroy_server_param.server_ = server_entity2;
    sm.DestroyServer(destroy_server_param);

    EXPECT_EQ(0, sm.scenes_size());
    EXPECT_FALSE(registry.valid(server_entity1));
    EXPECT_FALSE(registry.valid(scene_id1));
    EXPECT_FALSE(registry.valid(server_entity2));
    EXPECT_FALSE(registry.valid(scene_id2));

    EXPECT_EQ(0, sm.scenes_size(server1_param.scene_confid_));
    EXPECT_EQ(0, sm.scenes_size(server2_param.scene_confid_));
    EXPECT_EQ(sm.scenes_size(), sm.scenes_map_size());
}

TEST(GS, ServerScene2Sever)
{
    ScenesSystem sm;

    MakeGSParam cgs1;
    cgs1.node_id_ = 1;

    auto server_entity1 = MakeMainSceneNode( cgs1);

    MakeGSParam cgs2;
    cgs2.node_id_ = 2;
    auto server_entity2 = MakeMainSceneNode( cgs2);
    MakeGsSceneP server1_param;
    MakeGsSceneP server2_param;

    server1_param.scene_confid_ = 3;
    server1_param.node_ = server_entity1;

    server2_param.scene_confid_ = 2;
    server2_param.node_ = server_entity2;

    auto scene_id1 = sm.MakeScene2Gs(server1_param);
    auto scene_id2 = sm.MakeScene2Gs(server2_param);
    EXPECT_EQ(1, registry.get<ConfigSceneMap>(server_entity1).scenes_size());

    EXPECT_EQ(1, registry.get<ConfigSceneMap>(server_entity2).scenes_size());

    EXPECT_EQ(2, sm.scenes_size());
    EXPECT_EQ(sm.scenes_size(), sm.scenes_map_size());

    MoveServerScene2ServerSceneP move_scene_param;
    move_scene_param.from_server_ = server_entity1;
    move_scene_param.to_server_ = server_entity2;
    sm.MoveServerScene2ServerScene(move_scene_param);

    EXPECT_TRUE(registry.valid(server_entity1));
    EXPECT_TRUE(registry.valid(scene_id1));
    EXPECT_TRUE(registry.valid(server_entity2));
    EXPECT_TRUE(registry.valid(scene_id2));

    EXPECT_EQ(0, registry.get<ConfigSceneMap>(server_entity1).scenes_size());
    EXPECT_EQ(2, registry.get<ConfigSceneMap>(server_entity2).scenes_size());
    EXPECT_EQ(2, sm.scenes_size());
    EXPECT_EQ(1, sm.scenes_size(server1_param.scene_confid_));
    EXPECT_EQ(1, sm.scenes_size(server2_param.scene_confid_));

}

TEST(GS, PlayerLeaveEnterScene)
{
    ScenesSystem sm;
    MakeGSParam cgs1;
    cgs1.node_id_ = 1;

    auto server_entity1 = MakeMainSceneNode( cgs1);

    MakeGSParam cgs2;
    cgs2.node_id_ = 2;
    auto server_entity2 = MakeMainSceneNode( cgs2);

    MakeGsSceneP server1_param;
    MakeGsSceneP server2_param;

    server1_param.scene_confid_ = 3;
    server1_param.node_ = server_entity1;
;
    server2_param.scene_confid_ = 2;
    server2_param.node_ = server_entity2;

    auto scene_id1 = sm.MakeScene2Gs(server1_param);
    auto scene_id2 = sm.MakeScene2Gs(server2_param);

    EnterSceneParam enter_param1;
    enter_param1.scene_ = scene_id1;
    
    EnterSceneParam enter_param2;
    enter_param2.scene_ = scene_id2;

    uint32_t player_size = 100;
    EntitySet player_entities_set1;
    EntitySet player_entities_set2;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto pe = registry.create();
        
        if (i % 2 == 0)
        {
            player_entities_set1.emplace(pe);
            enter_param1.enterer_ = pe;
            sm.EnterScene(enter_param1);
        }
        else
        {
            player_entities_set2.emplace(pe);
            enter_param2.enterer_ = pe;
            sm.EnterScene(enter_param2);
        }
    }

    auto& scenes_players1 = registry.get<ScenePlayers>(scene_id1);
    auto& scenes_players2 = registry.get<ScenePlayers>(scene_id2);
    for (auto&it : player_entities_set1)
    {
        EXPECT_TRUE(scenes_players1.find(it) != scenes_players1.end());
        EXPECT_TRUE(registry.get<SceneEntity>(it).scene_entity() == scene_id1);
    }
    for (auto& it : player_entities_set2)
    {
        EXPECT_TRUE(scenes_players2.find(it) != scenes_players2.end());
        EXPECT_TRUE(registry.get<SceneEntity>(it).scene_entity() == scene_id2);
    }
    EXPECT_EQ(registry.get<GsNodePlayerInfoPtr>(server_entity1)->player_size(), player_size / 2);
    EXPECT_EQ(registry.get<GsNodePlayerInfoPtr>(server_entity2)->player_size(), player_size / 2);
    LeaveSceneParam leave_param1;
    for (auto& it : player_entities_set1)
    {
        leave_param1.leaver_ = it;
        sm.LeaveScene(leave_param1);
        EXPECT_FALSE(scenes_players1.find(it) != scenes_players1.end());
        EXPECT_EQ(registry.try_get<SceneEntity>(it), nullptr);
    }
    EXPECT_EQ(registry.get<GsNodePlayerInfoPtr>(server_entity1)->player_size(), 0);

    LeaveSceneParam leave_param2;
    for (auto& it : player_entities_set2)
    {
        leave_param2.leaver_ = it;
        sm.LeaveScene(leave_param2);
        EXPECT_FALSE(scenes_players2.find(it) != scenes_players2.end());
        EXPECT_EQ(registry.try_get<SceneEntity>(it), nullptr);
    }
    
    EXPECT_EQ(registry.get<GsNodePlayerInfoPtr>(server_entity2)->player_size(), 0);
    auto& scenes_players11 = registry.get<ScenePlayers>(scene_id1);
    auto& scenes_players22 = registry.get<ScenePlayers>(scene_id2);
    EXPECT_TRUE(scenes_players11.empty());
    EXPECT_TRUE(scenes_players22.empty());
}

TEST(GS, MainTainWeightRoundRobinMainScene)
{
    registry.clear();
    ScenesSystem sm;
    ServerNodeSystem snsys;
    EntitySet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 2;
    MakeGSParam cgs1;
    EntitySet scene_entities;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.node_id_ = i;
        server_entities.emplace(MakeMainSceneNode( cgs1));
    }

    MakeGsSceneP make_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_confid_ = i;
        for (auto& it : server_entities)
        {
            make_server_scene_param.node_ = it;
            auto e = sm.MakeScene2Gs(make_server_scene_param);
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
            auto p_e = registry.create();
            enter_param1.enterer_ = p_e;
            enter_param1.scene_ = it;
            player_scene1.emplace(enter_param1.enterer_, enter_param1.scene_);
            sm.EnterScene(enter_param1);
        }
    }

    MaintainServerParam maintain;
    maintain.maintain_entity_ = *server_entities.begin();
    snsys.ServerMaintain(registry, maintain);

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;
    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_confid_ = scene_config_id0;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = snsys.GetWeightRoundRobinMainScene(weight_round_robin_scene);
        EXPECT_TRUE(can_enter != entt::null);
    }
}

TEST(GS, CompelChangeScene)
{
    ScenesSystem sm;
    MakeGSParam cgs1;
    cgs1.node_id_ = 1;

    auto server_entity1 = MakeMainSceneNode( cgs1);

    MakeGSParam cgs2;
    cgs2.node_id_ = 2;
    auto server_entity2 = MakeMainSceneNode( cgs2);

    MakeGsSceneP server1_param;
    MakeGsSceneP server2_param;

    server1_param.scene_confid_ = 2;
    server1_param.node_ = server_entity1;

    server2_param.scene_confid_ = 2;
    server2_param.node_ = server_entity2;

    auto scene_id1 = sm.MakeScene2Gs(server1_param);
    auto scene_id2 = sm.MakeScene2Gs(server2_param);

    EnterSceneParam enter_param1;
    enter_param1.scene_ = scene_id1;

    EnterSceneParam enter_param2;
    enter_param2.scene_ = scene_id2;

    uint32_t player_size = 100;
    EntitySet player_entities_set1;
    EntitySet player_entities_set2;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto pe = registry.create();
        player_entities_set1.emplace(pe);
        enter_param1.enterer_ = pe;
        sm.EnterScene(enter_param1);
    }

    CompelChangeSceneParam compel_change_param1;
    compel_change_param1.new_server_ = server_entity2;
    compel_change_param1.scene_confid_ = server2_param.scene_confid_;
    for (auto& it : player_entities_set1)
    {
        compel_change_param1.compel_change_entity_ = it;
        sm.CompelChangeScene(compel_change_param1);
        EXPECT_TRUE(registry.try_get<SceneEntity>(it)->scene_entity() == scene_id2);
    }
    EXPECT_EQ(registry.get<GsNodePlayerInfoPtr>(server_entity1)->player_size(), 0);
    EXPECT_EQ(registry.get<GsNodePlayerInfoPtr>(server_entity2)->player_size(), player_entities_set1.size());
    auto& scenes_players11 = registry.get<ScenePlayers>(scene_id1);
    auto& scenes_players22 = registry.get<ScenePlayers>(scene_id2);
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
    MakeGSParam cgs1;
    EntitySet scene_entities;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.node_id_ = i;
        server_entities.emplace(MakeMainSceneNode( cgs1));
    }

    MakeGsSceneP make_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_confid_ = i;
        for (auto& it : server_entities)
        {
            make_server_scene_param.node_ = it;
            auto e = sm.MakeScene2Gs(make_server_scene_param);
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
            auto p_e = registry.create();
            enter_param1.enterer_ = p_e;
            enter_param1.scene_ = it;
            player_scene1.emplace(enter_param1.enterer_, enter_param1.scene_);
            sm.EnterScene(enter_param1);
        }
    }

    ServerCrashParam crash1;
    crash1.crash_entity_ = *server_entities.begin();
    snsys.ServerCrashed(registry, crash1);

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;
    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_confid_ = scene_config_id0;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = snsys.GetWeightRoundRobinMainScene(weight_round_robin_scene);
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
    MakeGSParam cgs1;
    EntitySet scene_entities;
    entt::entity first_scene_id = entt::null;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.node_id_ = i;
        server_entities.emplace(MakeMainSceneNode( cgs1));
    }

    MakeGsSceneP make_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_confid_ = i;
        for (auto& it : server_entities)
        {
            make_server_scene_param.node_ = it;
            auto e = sm.MakeScene2Gs(make_server_scene_param);
            scene_entities.emplace(e);
            if (first_scene_id == entt::null)
            {
                first_scene_id = e;
            }
        }
    }

    uint32_t player_size = 1000;

    std::unordered_map<entt::entity, entt::entity> player_scene1;

    EnterSceneParam enter_param1;

    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto p_e = registry.create();
        enter_param1.enterer_ = p_e;
        enter_param1.scene_ = first_scene_id;
        player_scene1.emplace(enter_param1.enterer_, enter_param1.scene_);
        sm.EnterScene(enter_param1);
    }

    ServerCrashParam crash1;
    crash1.crash_entity_ = *server_entities.begin();
    snsys.ServerCrashed(registry, crash1);

    ReplaceCrashServerParam replace_crash;
    replace_crash.cransh_server_ = *server_entities.begin();
    replace_crash.replace_server_ = *(++server_entities.begin());
    sm.ReplaceCrashServer(replace_crash);

    EXPECT_FALSE(registry.valid(replace_crash.cransh_server_));
    server_entities.erase(replace_crash.cransh_server_);
    for (auto& it : server_entities)
    {
        auto& server_scene =  registry.get<ConfigSceneMap>(it);
        EXPECT_EQ(server_scene.scenes_size(), scene_entities.size());
    }
    
}

TEST(GS, WeightRoundRobinMainScene)
{
    registry.clear();
    ScenesSystem sm;
    ServerNodeSystem snsys;
    EntitySet server_entities;
    uint32_t server_size = 10;
    uint32_t per_server_scene = 10;
    MakeGSParam cgs1;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.node_id_ = i;
        server_entities.emplace(MakeMainSceneNode( cgs1));
    }

    MakeGsSceneP make_server_scene_param;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_confid_ = i;
        for (auto& it :server_entities)
        {
            make_server_scene_param.node_ = it;
            sm.MakeScene2Gs(make_server_scene_param);
        }        
    }

    auto enter_leave_lambda = [&server_entities, server_size, per_server_scene, &sm, &snsys]()->void
    {
        uint32_t scene_config_id0 = 0;
        uint32_t scene_config_id1 = 1;
        GetSceneParam weight_round_robin_scene;
        weight_round_robin_scene.scene_confid_ = scene_config_id0;

        uint32_t player_size = 1000;

        std::unordered_map<entt::entity, entt::entity> player_scene1;
        EnterSceneParam enter_param1;

        EntitySet scene_sets;

        for (uint32_t i = 0; i < player_size; ++i)
        {
            auto can_enter = snsys.GetWeightRoundRobinMainScene(weight_round_robin_scene);
            auto p_e = registry.create();
            enter_param1.enterer_ = p_e;
            enter_param1.scene_ = can_enter;
            player_scene1.emplace(enter_param1.enterer_, can_enter);
            scene_sets.emplace(can_enter);
            sm.EnterScene(enter_param1);
        }

        uint32_t player_scene_id = 0;
        for (auto& it : player_scene1)
        {
            auto& pse = registry.get<SceneEntity>(it.first);
            EXPECT_TRUE(pse.scene_entity() == it.second);
            EXPECT_EQ(registry.get<SceneInfo>(pse.scene_entity()).scene_confid(), scene_config_id0);
        }

        std::unordered_map<entt::entity, entt::entity> player_scene2;
        weight_round_robin_scene.scene_confid_ = scene_config_id1;
        for (uint32_t i = 0; i < player_size; ++i)
        {
            auto can_enter = snsys.GetWeightRoundRobinMainScene(weight_round_robin_scene);
            auto p_e = registry.create();
            enter_param1.enterer_ = p_e;
            enter_param1.scene_ = can_enter;
            player_scene2.emplace(enter_param1.enterer_, enter_param1.scene_);
            scene_sets.emplace(can_enter);
            sm.EnterScene(enter_param1);
        }
        player_scene_id = 0;
        for (auto& it : player_scene2)
        {
            auto& pse = registry.get<SceneEntity>(it.first);
            EXPECT_TRUE(pse.scene_entity() == it.second);
            EXPECT_EQ(registry.get<SceneInfo>(pse.scene_entity()).scene_confid(), scene_config_id1);
        }

        std::size_t server_player_size = player_size * 2 / server_size;


        for (auto& it : server_entities)
        {
            auto& ps = registry.get<GsNodePlayerInfoPtr>(it);
            EXPECT_EQ((*ps).player_size(), server_player_size);
        }
        EXPECT_EQ(scene_sets.size(), std::size_t(2 * per_server_scene));

        LeaveSceneParam leave_scene;
        for (auto& it : player_scene1)
        {
            auto& pse = registry.get<SceneEntity>(it.first);
            leave_scene.leaver_ = it.first;
            sm.LeaveScene(leave_scene);
        }
        for (auto& it : player_scene2)
        {
            auto& pse = registry.get<SceneEntity>(it.first);
            leave_scene.leaver_ = it.first;
            sm.LeaveScene(leave_scene);
        }
        for (auto& it : server_entities)
        {
            auto& ps = registry.get<GsNodePlayerInfoPtr>(it);
            EXPECT_EQ((*ps).player_size(), 0);
        }
        for (auto& it : player_scene1)
        {
            EXPECT_EQ(registry.get<ScenePlayers>(it.second).size(), 0);
        }
        for (auto& it : player_scene2)
        {
            EXPECT_EQ(registry.get<ScenePlayers>(it.second).size(), 0);
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
    registry.clear();
    ScenesSystem sm;
    ServerNodeSystem snsys;
    EntitySet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 10;
    MakeGSParam cgs1;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.node_id_ = i;
        server_entities.emplace(MakeMainSceneNode( cgs1));
    }

    MakeGsSceneP make_server_scene_param;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_confid_ = i;
        for (auto& it : server_entities)
        {
            make_server_scene_param.node_ = it;
            sm.MakeScene2Gs(make_server_scene_param);
        }
    }
    ServerPressureParam pressure1;
    pressure1.server_ = *server_entities.begin();
    snsys.ServerEnterPressure(registry, pressure1);
    

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;

    GetSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_confid_ = scene_config_id0;

    std::unordered_map<entt::entity, entt::entity> player_scene1;

    EnterSceneParam enter_param1;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = snsys.GetWeightRoundRobinMainScene(weight_round_robin_scene);
        auto p_e = registry.create();
        enter_param1.enterer_ = p_e;
        enter_param1.scene_ = can_enter;
        player_scene1.emplace(enter_param1.enterer_, enter_param1.scene_);
        sm.EnterScene(enter_param1);
    }

    uint32_t player_scene_id = 0;

    snsys.ServerEnterNoPressure(registry, pressure1);

    std::unordered_map<entt::entity, entt::entity> player_scene2;
    weight_round_robin_scene.scene_confid_ = scene_config_id1;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = snsys.GetWeightRoundRobinMainScene(weight_round_robin_scene);
        auto p_e = registry.create();
        enter_param1.enterer_ = p_e;
        enter_param1.scene_ = can_enter;
        player_scene2.emplace(enter_param1.enterer_, enter_param1.scene_);
        sm.EnterScene(enter_param1);
    }

    
}

TEST(GS, GetNotFullMainSceneSceneFull)
{
	registry.clear();
	ScenesSystem sm;
	ServerNodeSystem snsys;
	EntitySet server_entities;
	uint32_t server_size = 10;
	uint32_t per_server_scene = 10;
	MakeGSParam cgs1;

	for (uint32_t i = 0; i < server_size; ++i)
	{
		cgs1.node_id_ = i;
		server_entities.emplace(MakeMainSceneNode( cgs1));
	}

	MakeGsSceneP make_server_scene_param;

	for (uint32_t i = 0; i < per_server_scene; ++i)
	{
		make_server_scene_param.scene_confid_ = i;
		for (auto& it : server_entities)
		{
			make_server_scene_param.node_ = it;
			sm.MakeScene2Gs(make_server_scene_param);
            sm.MakeScene2Gs(make_server_scene_param);
		}
	}

	auto enter_leave_lambda = [&server_entities, server_size, per_server_scene, &sm, &snsys]()->void
	{
		uint32_t scene_config_id0 = 0;
		uint32_t scene_config_id1 = 1;
		GetSceneParam weight_round_robin_scene;
		weight_round_robin_scene.scene_confid_ = scene_config_id0;

		uint32_t player_size = 1001;

		std::unordered_map<entt::entity, entt::entity> player_scene1;
		EnterSceneParam enter_param1;

		EntitySet scene_sets;

		for (uint32_t i = 0; i < player_size; ++i)
		{
			auto can_enter = snsys.GetMainSceneNotFull(weight_round_robin_scene);
            if (can_enter == entt::null)
            {
                continue;
            }
			auto p_e = registry.create();
			enter_param1.enterer_ = p_e;
			enter_param1.scene_ = can_enter;
			player_scene1.emplace(enter_param1.enterer_, can_enter);
			scene_sets.emplace(can_enter);
			sm.EnterScene(enter_param1);
		}

		uint32_t player_scene_id = 0;
		for (auto& it : player_scene1)
		{
			auto& pse = registry.get<SceneEntity>(it.first);
			EXPECT_TRUE(pse.scene_entity() == it.second);
			EXPECT_EQ(registry.get<SceneInfo>(pse.scene_entity()).scene_confid(), scene_config_id0);
		}

		std::unordered_map<entt::entity, entt::entity> player_scene2;
		weight_round_robin_scene.scene_confid_ = scene_config_id1;
		for (uint32_t i = 0; i < player_size; ++i)
		{
			auto can_enter = snsys.GetMainSceneNotFull(weight_round_robin_scene);
			auto p_e = registry.create();
			enter_param1.enterer_ = p_e;
			enter_param1.scene_ = can_enter;
			player_scene2.emplace(enter_param1.enterer_, enter_param1.scene_);
			scene_sets.emplace(can_enter);
			sm.EnterScene(enter_param1);
		}
		player_scene_id = 0;
		for (auto& it : player_scene2)
		{
			auto& pse = registry.get<SceneEntity>(it.first);
			EXPECT_TRUE(pse.scene_entity() == it.second);
			EXPECT_EQ(registry.get<SceneInfo>(pse.scene_entity()).scene_confid(), scene_config_id1);
		}

		std::size_t server_player_size = player_size * 2 / server_size;
        std::size_t remain_server_size = player_size * 2 - kMaxScenePlayerSize * 2;
		for (auto& it : server_entities)
		{
			auto& ps = registry.get<GsNodePlayerInfoPtr>(it);
            //todo if (ps->node_id() == 9)
            {
                EXPECT_EQ((*ps).player_size(), kMaxServerPlayerSize);
            }
            //todo if else if (ps->node_id() == 8)
            {
                EXPECT_EQ((*ps).player_size(), remain_server_size);
            }
            //todo if else
            {
                EXPECT_EQ((*ps).player_size(), 0);
            }
		}
		EXPECT_EQ(scene_sets.size(), std::size_t(4));

		LeaveSceneParam leave_scene;
		for (auto& it : player_scene1)
		{
			auto& pse = registry.get<SceneEntity>(it.first);
			leave_scene.leaver_ = it.first;
			sm.LeaveScene(leave_scene);
		}
		for (auto& it : player_scene2)
		{
			auto& pse = registry.get<SceneEntity>(it.first);
			leave_scene.leaver_ = it.first;
			sm.LeaveScene(leave_scene);
		}
		for (auto& it : server_entities)
		{
			auto& ps = registry.get<GsNodePlayerInfoPtr>(it);
			EXPECT_EQ((*ps).player_size(), 0);
		}
		for (auto& it : player_scene1)
		{
			EXPECT_EQ(registry.get<ScenePlayers>(it.second).size(), 0);
		}
		for (auto& it : player_scene2)
		{
			EXPECT_EQ(registry.get<ScenePlayers>(it.second).size(), 0);
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
    registry.clear();
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
