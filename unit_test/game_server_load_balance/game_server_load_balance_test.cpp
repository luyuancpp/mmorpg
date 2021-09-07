#include <gtest/gtest.h>

#include "src/common_type/common_type.h"
#include "src/factories/scene_factories.hpp"
#include "src/game_logic/comp/server_list.hpp"
#include "src/game_logic/game_registry.h"
#include "src/sys/scene_sys.hpp"

using namespace  master;
using namespace  common;

uint32_t scene_config_size = 50;
uint32_t per_scene_config_size = 2;

TEST(GameServer, CreateMainScene)
{
    MakeScenes();
    MakeSceneParam param;
    auto& c = reg().get<common::Scenes>(scenes_entity());
    for (uint32_t i = 0; i < scene_config_size; ++i)
    {
        param.scene_config_id_ = i;
        for (uint32_t j = 0; j < per_scene_config_size; ++j)
        {
            MakeMainScene(reg(), param);
        }
        EXPECT_EQ(c.scene_config_size(i), std::size_t(per_scene_config_size));
    }
    EXPECT_EQ(c.scenes_size(), std::size_t(scene_config_size * per_scene_config_size));
    EXPECT_EQ(reg().get<common::SceneMap>(scenes_entity()).size(), std::size_t(scene_config_size * per_scene_config_size));
    reg().clear();
}

TEST(GameServer, MakeScene2Sever )
{
    MakeScenes();

    MakeGameServerParam param1;
    param1.server_id_ = 1;

    auto server_entity1 = MakeGameServer(reg(), param1);

    MakeGameServerParam param2;
    param2.server_id_ = 1;
    auto server_entity2 = MakeGameServer(reg(), param2);

    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;

    server1_param.scene_config_id_ = 2;
    server1_param.server_entity_ = server_entity1;

    server2_param.scene_config_id_ = 3;
    server2_param.server_entity_ = server_entity2;

    MakeScene2GameServer(reg(), server1_param);
    MakeScene2GameServer(reg(), server2_param);

    auto& scenes = reg().get<common::Scenes>(scenes_entity());
    auto& server_data1 = *reg().get<common::GameServerDataPtr>(server_entity1);
    auto& scenes_id1 = reg().get<common::Scenes>(server_entity1);
 
    auto& server_data2 = *reg().get<common::GameServerDataPtr>(server_entity2);
    auto& scenes_id2 = reg().get<common::Scenes>(server_entity2);

    EXPECT_EQ(1, scenes_id1.scenes_size());
    EXPECT_EQ(server1_param.scene_config_id_, reg().get<common::SceneConfig>(scenes_id1.first_scene()).scene_config_id());
    EXPECT_EQ(1, scenes.scene_config_size(server1_param.scene_config_id_));
    EXPECT_EQ(server_data1.server_id(), param1.server_id_);

    EXPECT_EQ(1, scenes_id2.scenes_size());
    EXPECT_EQ(server2_param.scene_config_id_, reg().get<common::SceneConfig>(scenes_id2.first_scene()).scene_config_id());
    EXPECT_EQ(server_data2.server_id(), param2.server_id_);

    EXPECT_EQ(1, scenes.scene_config_size(server2_param.scene_config_id_));
    EXPECT_EQ(2, scenes.scenes_size());
    EXPECT_EQ(reg().get<common::Scenes>(scenes_entity()).scenes_size(), reg().get<common::SceneMap>(scenes_entity()).size());
    reg().clear();
}

TEST(GameServer, PutScene2Sever)
{
    MakeScenes();

    MakeGameServerParam param1;
    param1.server_id_ = 1;

    MakeSceneParam cparam;
    auto scene_entity = MakeMainScene(reg(), cparam);

    auto server_entity1 = MakeGameServer(reg(), param1);
        
    PutScene2GameServerParam put_param;
    put_param.scene_entity_ = scene_entity;
    put_param.server_entity_ = server_entity1;
    PutScene2GameServer(reg(), put_param);

    auto& scenes = reg().get<common::Scenes>(scenes_entity());
    EXPECT_EQ(1, scenes.scenes_size());
    EXPECT_EQ(1, scenes.scene_config_size(cparam.scene_config_id_));
    EXPECT_EQ(reg().get<common::Scenes>(scenes_entity()).scenes_size(), reg().get<common::SceneMap>(scenes_entity()).size());

    auto& server_scenes = reg().get<common::Scenes>(server_entity1);
    EXPECT_EQ(1, server_scenes.scenes_size());
    reg().clear();
}

TEST(GameServer, DestroyScene)
{
    MakeScenes();
    MakeGameServerParam param1;
    param1.server_id_ = 1;

    MakeSceneParam cparam;
    auto scene_entity = MakeMainScene(reg(), cparam);

    auto server_entity1 = MakeGameServer(reg(), param1);

    PutScene2GameServerParam put_param;
    put_param.scene_entity_ = scene_entity;
    put_param.server_entity_ = server_entity1;
    PutScene2GameServer(reg(), put_param);

    auto& scenes = reg().get<common::Scenes>(scenes_entity());
    EXPECT_EQ(1, scenes.scenes_size());
    EXPECT_EQ(1, scenes.scene_config_size(cparam.scene_config_id_));
    EXPECT_EQ(reg().get<common::Scenes>(scenes_entity()).scenes_size(), reg().get<common::SceneMap>(scenes_entity()).size());

    auto& server_scenes = reg().get<common::Scenes>(server_entity1);
    EXPECT_EQ(1, server_scenes.scenes_size());

    DestroySceneParam dparam;
    dparam.scene_entity_ = scene_entity;
    DestroyScene(reg(), dparam);
    EXPECT_TRUE(scenes.scenes_empty());
    EXPECT_TRUE(scenes.scene_config_empty(cparam.scene_config_id_));
    EXPECT_TRUE(server_scenes.scenes_empty());
    EXPECT_EQ(reg().get<common::Scenes>(scenes_entity()).scenes_size(), reg().get<common::SceneMap>(scenes_entity()).size());
    EXPECT_FALSE(reg().valid(scene_entity));
    reg().clear();
}

TEST(GameServer, DestroySever)
{
    MakeScenes();

    MakeGameServerParam param1;
    param1.server_id_ = 1;

    auto server_entity1 = MakeGameServer(reg(), param1);

    MakeGameServerParam param2;
    param2.server_id_ = 2;
    auto server_entity2 = MakeGameServer(reg(), param2);

    auto& server_data1 = *reg().get<common::GameServerDataPtr>(server_entity1);
    
    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;
    server1_param.scene_config_id_ = 3;
    server1_param.server_entity_ = server_entity1;

    server2_param.scene_config_id_ = 2;
    server2_param.server_entity_ = server_entity2;

    auto scene_id1 =  MakeScene2GameServer(reg(), server1_param);
    auto scene_id2 = MakeScene2GameServer(reg(), server2_param);

    auto& scenes_id1 = reg().get<common::Scenes>(server_entity1);

    auto& server_data2 = *reg().get<common::GameServerDataPtr>(server_entity2);

    EXPECT_EQ(1, scenes_id1.scenes_size());
    EXPECT_EQ(server_data1.server_id(), param1.server_id_);

    EXPECT_EQ(1, reg().get<common::Scenes>(server_entity2).scenes_size());
    EXPECT_EQ(server_data2.server_id(), param2.server_id_);

    auto& scenes = reg().get<common::Scenes>(scenes_entity());
    EXPECT_EQ(2, scenes.scenes_size());
    EXPECT_EQ(reg().get<common::Scenes>(scenes_entity()).scenes_size(), reg().get<common::SceneMap>(scenes_entity()).size());

    DestroyServerParam destroy_server_param;
    destroy_server_param.server_entity_ = server_entity1;
    DestroyServer(reg(), destroy_server_param);

    EXPECT_FALSE(reg().valid(server_entity1));
    EXPECT_FALSE(reg().valid(scene_id1));
    EXPECT_TRUE(reg().valid(server_entity2));
    EXPECT_TRUE(reg().valid(scene_id2));

    EXPECT_EQ(1, reg().get<common::Scenes>(server_entity2).scenes_size());
    EXPECT_EQ(1, scenes.scenes_size());
    EXPECT_EQ(0, scenes.scene_config_size(server1_param.scene_config_id_));
    EXPECT_EQ(1, scenes.scene_config_size(server2_param.scene_config_id_));

    destroy_server_param.server_entity_ = server_entity2;
    DestroyServer(reg(), destroy_server_param);

    EXPECT_EQ(0, scenes.scenes_size());
    EXPECT_FALSE(reg().valid(server_entity1));
    EXPECT_FALSE(reg().valid(scene_id1));
    EXPECT_FALSE(reg().valid(server_entity2));
    EXPECT_FALSE(reg().valid(scene_id2));

    EXPECT_EQ(0, scenes.scene_config_size(server1_param.scene_config_id_));
    EXPECT_EQ(0, scenes.scene_config_size(server2_param.scene_config_id_));
    EXPECT_EQ(reg().get<common::Scenes>(scenes_entity()).scenes_size(), reg().get<common::SceneMap>(scenes_entity()).size());
    reg().clear();
}

TEST(GameServer, ServerScene2Sever)
{
    MakeScenes();

    MakeGameServerParam cgs1;
    cgs1.server_id_ = 1;

    auto server_entity1 = MakeGameServer(reg(), cgs1);

    MakeGameServerParam cgs2;
    cgs2.server_id_ = 2;
    auto server_entity2 = MakeGameServer(reg(), cgs2);

   
    auto& server_data2 = *reg().get<common::GameServerDataPtr>(server_entity2);

    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;

    server1_param.scene_config_id_ = 3;
    server1_param.server_entity_ = server_entity1;

    server2_param.scene_config_id_ = 2;
    server2_param.server_entity_ = server_entity2;

    auto scene_id1 = MakeScene2GameServer(reg(), server1_param);
    auto scene_id2 = MakeScene2GameServer(reg(), server2_param);

    auto& server_data1 = *reg().get<common::GameServerDataPtr>(server_entity1);
    auto& scenes_id1 = reg().get<common::Scenes>(server_entity1);

    EXPECT_EQ(1, scenes_id1.scenes_size());
    EXPECT_EQ(server_data1.server_id(), cgs1.server_id_);

    EXPECT_EQ(1, reg().get<common::Scenes>(server_entity2).scenes_size());
    EXPECT_EQ(server_data2.server_id(), cgs2.server_id_);

    auto& scenes = reg().get<common::Scenes>(scenes_entity());
    EXPECT_EQ(2, scenes.scenes_size());
    EXPECT_EQ(reg().get<common::Scenes>(scenes_entity()).scenes_size(), reg().get<common::SceneMap>(scenes_entity()).size());

    EXPECT_EQ(reg().get<common::GameServerDataPtr>(scene_id1)->server_id(), cgs1.server_id_);
    EXPECT_EQ(reg().get<common::GameServerDataPtr>(scene_id2)->server_id(), cgs2.server_id_);

    MoveServerScene2ServerParam move_scene_param;
    move_scene_param.from_server_entity_ = server_entity1;
    move_scene_param.to_server_entity_ = server_entity2;
    MoveServerScene2Server(reg(), move_scene_param);

    EXPECT_TRUE(reg().valid(server_entity1));
    EXPECT_TRUE(reg().valid(scene_id1));
    EXPECT_TRUE(reg().valid(server_entity2));
    EXPECT_TRUE(reg().valid(scene_id2));

    EXPECT_EQ(0, reg().get<common::Scenes>(server_entity1).scenes_size());
    EXPECT_EQ(2, reg().get<common::Scenes>(server_entity2).scenes_size());
    EXPECT_EQ(2, scenes.scenes_size());
    EXPECT_EQ(1, scenes.scene_config_size(server1_param.scene_config_id_));
    EXPECT_EQ(1, scenes.scene_config_size(server2_param.scene_config_id_));
    EXPECT_EQ(reg().get<common::GameServerDataPtr>(scene_id1)->server_id(), cgs2.server_id_);
    EXPECT_EQ(reg().get<common::GameServerDataPtr>(scene_id2)->server_id(), cgs2.server_id_);
    EXPECT_EQ(reg().get<common::GameServerDataPtr>(server_entity1).use_count(), 1);
    reg().clear();
}

TEST(GameServer, PlayerLeaveEnterScene)
{
    MakeScenes();
    MakeGameServerParam cgs1;
    cgs1.server_id_ = 1;

    auto server_entity1 = MakeGameServer(reg(), cgs1);

    MakeGameServerParam cgs2;
    cgs2.server_id_ = 2;
    auto server_entity2 = MakeGameServer(reg(), cgs2);

    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;

    server1_param.scene_config_id_ = 3;
    server1_param.server_entity_ = server_entity1;
;
    server2_param.scene_config_id_ = 2;
    server2_param.server_entity_ = server_entity2;

    auto scene_id1 = MakeScene2GameServer(reg(), server1_param);
    auto scene_id2 = MakeScene2GameServer(reg(), server2_param);

    EnterSceneParam enter_param1;
    enter_param1.scene_entity_ = scene_id1;
    
    EnterSceneParam enter_param2;
    enter_param2.scene_entity_ = scene_id2;

    uint32_t player_size = 100;
    EntitiesUSet player_entities_set1;
    EntitiesUSet player_entities_set2;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto pe = reg().create();
        
        if (i % 2 == 0)
        {
            player_entities_set1.emplace(pe);
            enter_param1.enter_entity_ = pe;
            EnterScene(reg(), enter_param1);
        }
        else
        {
            player_entities_set2.emplace(pe);
            enter_param2.enter_entity_ = pe;
            EnterScene(reg(), enter_param2);
        }
    }

    auto& scenes_players1 = reg().get<common::PlayerEntities>(scene_id1);
    auto& scenes_players2 = reg().get<common::PlayerEntities>(scene_id2);
    for (auto&it : player_entities_set1)
    {
        EXPECT_TRUE(scenes_players1.find(it) != scenes_players1.end());
        EXPECT_TRUE(reg().get<common::SceneEntityId>(it).scene_entity() == scene_id1);
    }
    for (auto& it : player_entities_set2)
    {
        EXPECT_TRUE(scenes_players2.find(it) != scenes_players2.end());
        EXPECT_TRUE(reg().get<common::SceneEntityId>(it).scene_entity() == scene_id2);
    }
    EXPECT_EQ(reg().get<common::GameServerDataPtr>(server_entity1)->player_size(), player_size / 2);
    EXPECT_EQ(reg().get<common::GameServerDataPtr>(server_entity2)->player_size(), player_size / 2);
    LeaveSceneParam leave_param1;
    for (auto& it : player_entities_set1)
    {
        leave_param1.leave_entity_ = it;
        LeaveScene(reg(), leave_param1);
        EXPECT_FALSE(scenes_players1.find(it) != scenes_players1.end());
        EXPECT_EQ(reg().try_get<common::SceneEntityId>(it), nullptr);
    }
    EXPECT_EQ(reg().get<common::GameServerDataPtr>(server_entity1)->player_size(), 0);

    LeaveSceneParam leave_param2;
    for (auto& it : player_entities_set2)
    {
        leave_param2.leave_entity_ = it;
        LeaveScene(reg(), leave_param2);
        EXPECT_FALSE(scenes_players2.find(it) != scenes_players2.end());
        EXPECT_EQ(reg().try_get<common::SceneEntityId>(it), nullptr);
    }
    
    EXPECT_EQ(reg().get<common::GameServerDataPtr>(server_entity2)->player_size(), 0);
    auto& scenes_players11 = reg().get<common::PlayerEntities>(scene_id1);
    auto& scenes_players22 = reg().get<common::PlayerEntities>(scene_id2);
    EXPECT_TRUE(scenes_players11.empty());
    EXPECT_TRUE(scenes_players22.empty());
    reg().clear();
}

TEST(GameServer, MainTainWeightRoundRobinMainScene)
{
    MakeScenes();
    EntitiesUSet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 2;
    MakeGameServerParam cgs1;
    EntitiesUSet scene_entities;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.server_id_ = i;
        server_entities.emplace(MakeGameServer(reg(), cgs1));
    }

    MakeScene2GameServerParam make_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_config_id_ = i;
        for (auto& it : server_entities)
        {
            make_server_scene_param.server_entity_ = it;
            auto e = MakeScene2GameServer(reg(), make_server_scene_param);
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
            auto p_e = reg().create();
            enter_param1.enter_entity_ = p_e;
            enter_param1.scene_entity_ = it;
            player_scene1.emplace(enter_param1.enter_entity_, enter_param1.scene_entity_);
            EnterScene(reg(), enter_param1);
        }
    }

    MaintainServerParam maintain;
    maintain.maintain_server_entity_ = *server_entities.begin();
    ServerMaintain(reg(), maintain);

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;
    GetWeightRoundRobinSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_config_id_ = scene_config_id0;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = GetWeightRoundRobinMainScene(reg(), weight_round_robin_scene);
        EXPECT_TRUE(reg().get<common::GameServerDataPtr>(can_enter)->server_entity() != entt::null);
        EXPECT_TRUE(reg().get<common::GameServerDataPtr>(can_enter)->server_entity() != maintain.maintain_server_entity_);
    }

    reg().clear();
}

TEST(GameServer, CompelChangeScene)
{
    MakeScenes();
    MakeGameServerParam cgs1;
    cgs1.server_id_ = 1;

    auto server_entity1 = MakeGameServer(reg(), cgs1);

    MakeGameServerParam cgs2;
    cgs2.server_id_ = 2;
    auto server_entity2 = MakeGameServer(reg(), cgs2);

    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;

    server1_param.scene_config_id_ = 2;
    server1_param.server_entity_ = server_entity1;

    server2_param.scene_config_id_ = 2;
    server2_param.server_entity_ = server_entity2;

    auto scene_id1 = MakeScene2GameServer(reg(), server1_param);
    auto scene_id2 = MakeScene2GameServer(reg(), server2_param);

    EnterSceneParam enter_param1;
    enter_param1.scene_entity_ = scene_id1;

    EnterSceneParam enter_param2;
    enter_param2.scene_entity_ = scene_id2;

    uint32_t player_size = 100;
    EntitiesUSet player_entities_set1;
    EntitiesUSet player_entities_set2;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto pe = reg().create();
        player_entities_set1.emplace(pe);
        enter_param1.enter_entity_ = pe;
        EnterScene(reg(), enter_param1);
    }

    CompelChangeSceneParam compel_change_param1;
    compel_change_param1.new_server_entity_ = server_entity2;
    compel_change_param1.scene_config_id_ = server2_param.scene_config_id_;
    for (auto& it : player_entities_set1)
    {
        compel_change_param1.compel_change_entity_ = it;
        CompelChangeScene(reg(), compel_change_param1);
        EXPECT_TRUE(reg().try_get<common::SceneEntityId>(it)->scene_entity() == scene_id2);
    }
    EXPECT_EQ(reg().get<common::GameServerDataPtr>(server_entity1)->player_size(), 0);
    EXPECT_EQ(reg().get<common::GameServerDataPtr>(server_entity2)->player_size(), player_entities_set1.size());
    auto& scenes_players11 = reg().get<common::PlayerEntities>(scene_id1);
    auto& scenes_players22 = reg().get<common::PlayerEntities>(scene_id2);
    EXPECT_TRUE(scenes_players11.empty());
    EXPECT_EQ(scenes_players22.size(), player_entities_set1.size());
    reg().clear();
}


TEST(GameServer, CrashWeightRoundRobinMainScene)
{
    MakeScenes();
    EntitiesUSet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 2;
    MakeGameServerParam cgs1;
    EntitiesUSet scene_entities;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.server_id_ = i;
        server_entities.emplace(MakeGameServer(reg(), cgs1));
    }

    MakeScene2GameServerParam make_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_config_id_ = i;
        for (auto& it : server_entities)
        {
            make_server_scene_param.server_entity_ = it;
            auto e = MakeScene2GameServer(reg(), make_server_scene_param);
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
            auto p_e = reg().create();
            enter_param1.enter_entity_ = p_e;
            enter_param1.scene_entity_ = it;
            player_scene1.emplace(enter_param1.enter_entity_, enter_param1.scene_entity_);
            EnterScene(reg(), enter_param1);
        }
    }

    ServerCrashParam crash1;
    crash1.crash_server_entity_ = *server_entities.begin();
    ServerCrashed(reg(), crash1);

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;
    GetWeightRoundRobinSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_config_id_ = scene_config_id0;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto can_enter = GetWeightRoundRobinMainScene(reg(), weight_round_robin_scene);
        EXPECT_TRUE(reg().get<common::GameServerDataPtr>(can_enter)->server_entity() != entt::null);
        EXPECT_TRUE(reg().get<common::GameServerDataPtr>(can_enter)->server_entity() != crash1.crash_server_entity_);
    }

    reg().clear();
}

//崩溃时候的消息不能处理
TEST(GameServer, CrashMovePlayer2NewServer)
{
    MakeScenes();
    EntitiesUSet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 2;
    MakeGameServerParam cgs1;
    EntitiesUSet scene_entities;
    entt::entity first_scene = entt::null;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.server_id_ = i;
        server_entities.emplace(MakeGameServer(reg(), cgs1));
    }

    MakeScene2GameServerParam make_server_scene_param;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_config_id_ = i;
        for (auto& it : server_entities)
        {
            make_server_scene_param.server_entity_ = it;
            auto e = MakeScene2GameServer(reg(), make_server_scene_param);
            scene_entities.emplace(e);
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
        auto p_e = reg().create();
        enter_param1.enter_entity_ = p_e;
        enter_param1.scene_entity_ = first_scene;
        player_scene1.emplace(enter_param1.enter_entity_, enter_param1.scene_entity_);
        EnterScene(reg(), enter_param1);
    }

    ServerCrashParam crash1;
    crash1.crash_server_entity_ = *server_entities.begin();
    ServerCrashed(reg(), crash1);

    ReplaceCrashServerParam replace_crash;
    replace_crash.cransh_server_entity_ = *server_entities.begin();
    replace_crash.replace_server_entity_ = *(++server_entities.begin());
    ReplaceCrashServer(reg(), replace_crash);

    EXPECT_FALSE(reg().valid(replace_crash.cransh_server_entity_));
    server_entities.erase(replace_crash.cransh_server_entity_);
    for (auto& it : server_entities)
    {
        auto& server_scene =  reg().get<Scenes>(it);
        EXPECT_EQ(server_scene.scenes_size(), scene_entities.size());
    }

    auto& eq_server_data = reg().get<GameServerDataPtr>(replace_crash.replace_server_entity_);
    EXPECT_EQ(1, eq_server_data->server_id());
    for (auto& it : scene_entities)
    {
        auto& server_data = reg().get<GameServerDataPtr>(it);
        EXPECT_EQ(server_data->server_id(), eq_server_data->server_id());
    }
    for (auto& it : player_scene1)
    {
        auto& player_scene_entity = reg().get<common::SceneEntityId>(it.first);
        auto& server_data = reg().get<GameServerDataPtr>(player_scene_entity.scene_entity());
        EXPECT_EQ(server_data->server_id(), eq_server_data->server_id());
    }
    
    reg().clear();
}

TEST(GameServer, WeightRoundRobinMainScene)
{
    MakeScenes();
    EntitiesUSet server_entities;
    uint32_t server_size = 10;
    uint32_t per_server_scene = 10;
    MakeGameServerParam cgs1;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.server_id_ = i;
        server_entities.emplace(MakeGameServer(reg(), cgs1));
    }

    MakeScene2GameServerParam make_server_scene_param;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_config_id_ = i;
        for (auto& it :server_entities)
        {
            make_server_scene_param.server_entity_ = it;
            MakeScene2GameServer(reg(), make_server_scene_param);
        }        
    }

    auto enter_leave_lambda = [&server_entities, server_size, per_server_scene]()->void
    {
        uint32_t scene_config_id0 = 0;
        uint32_t scene_config_id1 = 1;
        GetWeightRoundRobinSceneParam weight_round_robin_scene;
        weight_round_robin_scene.scene_config_id_ = scene_config_id0;

        uint32_t player_size = 1000;

        std::unordered_map<entt::entity, entt::entity> player_scene1;
        EnterSceneParam enter_param1;

        EntitiesUSet scene_sets;

        for (uint32_t i = 0; i < player_size; ++i)
        {
            auto can_enter = GetWeightRoundRobinMainScene(reg(), weight_round_robin_scene);
            auto p_e = reg().create();
            enter_param1.enter_entity_ = p_e;
            enter_param1.scene_entity_ = can_enter;
            player_scene1.emplace(enter_param1.enter_entity_, can_enter);
            scene_sets.emplace(can_enter);
            EnterScene(reg(), enter_param1);
        }

        uint32_t player_scene_id = 0;
        for (auto& it : player_scene1)
        {
            auto& pse = reg().get<common::SceneEntityId>(it.first);
            EXPECT_TRUE(pse.scene_entity() == it.second);
            EXPECT_EQ(reg().get<common::SceneConfig>(pse.scene_entity()).scene_config_id(), scene_config_id0);
        }

        std::unordered_map<entt::entity, entt::entity> player_scene2;
        weight_round_robin_scene.scene_config_id_ = scene_config_id1;
        for (uint32_t i = 0; i < player_size; ++i)
        {
            auto can_enter = GetWeightRoundRobinMainScene(reg(), weight_round_robin_scene);
            auto p_e = reg().create();
            enter_param1.enter_entity_ = p_e;
            enter_param1.scene_entity_ = can_enter;
            player_scene2.emplace(enter_param1.enter_entity_, enter_param1.scene_entity_);
            scene_sets.emplace(can_enter);
            EnterScene(reg(), enter_param1);
        }
        player_scene_id = 0;
        for (auto& it : player_scene2)
        {
            auto& pse = reg().get<common::SceneEntityId>(it.first);
            EXPECT_TRUE(pse.scene_entity() == it.second);
            EXPECT_EQ(reg().get<common::SceneConfig>(pse.scene_entity()).scene_config_id(), scene_config_id1);
        }

        std::size_t server_player_size = player_size * 2 / server_size;

        for (auto& it : server_entities)
        {
            auto& ps = reg().get<common::GameServerDataPtr>(it);
            EXPECT_EQ((*ps).player_size(), server_player_size);
        }
        EXPECT_EQ(scene_sets.size(), std::size_t(2 * per_server_scene));

        LeaveSceneParam leave_scene;
        for (auto& it : player_scene1)
        {
            auto& pse = reg().get<common::SceneEntityId>(it.first);
            leave_scene.leave_entity_ = it.first;
            LeaveScene(reg(), leave_scene);
        }
        for (auto& it : player_scene2)
        {
            auto& pse = reg().get<common::SceneEntityId>(it.first);
            leave_scene.leave_entity_ = it.first;
            LeaveScene(reg(), leave_scene);
        }
        for (auto& it : server_entities)
        {
            auto& ps = reg().get<common::GameServerDataPtr>(it);
            EXPECT_EQ((*ps).player_size(), 0);
        }
        for (auto& it : player_scene1)
        {
            EXPECT_EQ(reg().get<common::PlayerEntities>(it.second).size(), 0);
        }
        for (auto& it : player_scene2)
        {
            EXPECT_EQ(reg().get<common::PlayerEntities>(it.second).size(), 0);
        }
    };
    for (uint32_t i = 0; i < 2; ++i)
    {
        enter_leave_lambda();
    }    
    //leave 
    reg().clear();
}

TEST(GameServer, ServerEnterLeavePressure)
{
    MakeScenes();
    EntitiesUSet server_entities;
    uint32_t server_size = 2;
    uint32_t per_server_scene = 10;
    MakeGameServerParam cgs1;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.server_id_ = i;
        server_entities.emplace(MakeGameServer(reg(), cgs1));
    }

    MakeScene2GameServerParam make_server_scene_param;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_config_id_ = i;
        for (auto& it : server_entities)
        {
            make_server_scene_param.server_entity_ = it;
            MakeScene2GameServer(reg(), make_server_scene_param);
        }
    }
    ServerPressureParam pressure1;
    pressure1.server_entity_ = *server_entities.begin();
    ServerEnterPressure(reg(), pressure1);
    

    uint32_t scene_config_id0 = 0;
    uint32_t scene_config_id1 = 1;

    GetWeightRoundRobinSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_config_id_ = scene_config_id0;

    std::unordered_map<entt::entity, entt::entity> player_scene1;

    EnterSceneParam enter_param1;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = GetWeightRoundRobinMainScene(reg(), weight_round_robin_scene);
        auto p_e = reg().create();
        enter_param1.enter_entity_ = p_e;
        enter_param1.scene_entity_ = can_enter;
        player_scene1.emplace(enter_param1.enter_entity_, enter_param1.scene_entity_);
        EnterScene(reg(), enter_param1);
    }

    uint32_t player_scene_id = 0;
    for (auto& it : player_scene1)
    {
        auto& psr = reg().get<common::GameServerDataPtr>(it.second);
        EXPECT_TRUE(psr->server_entity() != pressure1.server_entity_);
    }

    ServerEnterNoPressure(reg(), pressure1);

    std::unordered_map<entt::entity, entt::entity> player_scene2;
    weight_round_robin_scene.scene_config_id_ = scene_config_id1;
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = GetWeightRoundRobinMainScene(reg(), weight_round_robin_scene);
        auto p_e = reg().create();
        enter_param1.enter_entity_ = p_e;
        enter_param1.scene_entity_ = can_enter;
        player_scene2.emplace(enter_param1.enter_entity_, enter_param1.scene_entity_);
        EnterScene(reg(), enter_param1);
    }
    player_scene_id = 0;
    for (auto& it : player_scene2)
    {
        auto& psr = reg().get<common::GameServerDataPtr>(it.second);
        EXPECT_TRUE(psr->server_entity() == pressure1.server_entity_);
    }
    reg().clear();
}


TEST(GameServer, CreateDungeon)
{
}

TEST(GameServer, Route)
{
    MakeScenes();

    reg().clear();
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
