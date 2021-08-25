#include <gtest/gtest.h>

#include "src/common_type/common_type.h"
#include "src/factories/scene_factories.hpp"
#include "src/game/game_client.h"
#include "src/game_logic/comp/server_list.hpp"
#include "src/game_logic/game_registry.h"
#include "src/sys/scene_sys.hpp"

using namespace  master;
using namespace  common;

uint32_t scene_config_size = 50;
uint32_t per_scene_config_size = 10;

TEST(GameServer, CreateMainScene)
{
    entt::registry reg;
    auto e = MakeScenes(reg);
    MakeSceneParam param;
    param.scene_map_entity_ = e;
    auto& c = reg.get<common::Scenes>(e);
    for (uint32_t i = 0; i < scene_config_size; ++i)
    {
        param.scene_config_id_ = i;
        for (uint32_t j = 0; j < per_scene_config_size; ++j)
        {
            MakeMainScene(reg, param);
        }
        EXPECT_EQ(c.scenes_group_[i].size(), std::size_t(per_scene_config_size));
    }
    EXPECT_EQ(c.scenes_.size(), std::size_t(scene_config_size * per_scene_config_size));
}

TEST(GameServer, MakeScene2Sever )
{
    entt::registry reg;
    auto e = MakeScenes(reg);

    MakeGameServerParam param1;
    param1.server_id_ = 1;

    auto server_entity1 = MakeGameServer(reg, param1);

    MakeGameServerParam param2;
    param2.server_id_ = 1;
    auto server_entity2 = MakeGameServer(reg, param2);

    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;

    server1_param.scene_map_entity_ = e;
    server1_param.scene_config_id_ = 2;
    server1_param.server_entity_ = server_entity1;

    server2_param.scene_map_entity_ = e;
    server2_param.scene_config_id_ = 3;
    server2_param.server_entity_ = server_entity2;

    MakeScene2GameServer(reg, server1_param);
    MakeScene2GameServer(reg, server2_param);

    auto& scenes = reg.get<common::Scenes>(e);
    auto& server_data1 = *reg.get<common::GameServerDataPtr>(server_entity1);
    auto& scenes_id1 = reg.get<common::SceneIds>(server_entity1);
 
    auto& server_data2 = *reg.get<common::GameServerDataPtr>(server_entity2);
    auto& scenes_id2 = reg.get<common::SceneIds>(server_entity2);

    EXPECT_EQ(1, scenes_id1.size());
    EXPECT_EQ(server1_param.scene_config_id_, reg.get<common::SceneConfigId>(*scenes_id1.begin()).scene_config_id_);
    EXPECT_EQ(1, scenes.scenes_group_[server1_param.scene_config_id_].size());
    EXPECT_EQ(server_data1.server_id_, param1.server_id_);

    EXPECT_EQ(1, scenes_id2.size());
    EXPECT_EQ(server2_param.scene_config_id_, reg.get<common::SceneConfigId>(*scenes_id2.begin()).scene_config_id_);
    EXPECT_EQ(server_data2.server_id_, param2.server_id_);

    EXPECT_EQ(1, scenes.scenes_group_[server2_param.scene_config_id_].size());
    EXPECT_EQ(2, scenes.scenes_.size());
}

TEST(GameServer, PutScene2Sever)
{
    entt::registry reg;
    auto e = MakeScenes(reg);

    MakeGameServerParam param1;
    param1.server_id_ = 1;

    MakeSceneParam cparam;
    cparam.scene_map_entity_ = e;
    auto scene_entity = MakeMainScene(reg, cparam);

    auto server_entity1 = MakeGameServer(reg, param1);
        
    PutScene2GameServerParam put_param;
    put_param.scene_entity_ = scene_entity;
    put_param.server_entity_ = server_entity1;
    PutScene2GameServer(reg, put_param);

    auto& scenes = reg.get<common::Scenes>(e);
    EXPECT_EQ(1, scenes.scenes_.size());
    EXPECT_EQ(1, scenes.scenes_group_[cparam.scene_config_id_].size());

    auto& server_scenes = reg.get<common::SceneIds>(server_entity1);
    EXPECT_EQ(1, server_scenes.size());

}

TEST(GameServer, DestroyScene)
{
    entt::registry reg;
    auto e = MakeScenes(reg);

    MakeGameServerParam param1;
    param1.server_id_ = 1;

    MakeSceneParam cparam;
    cparam.scene_map_entity_ = e;
    auto scene_entity = MakeMainScene(reg, cparam);

    auto server_entity1 = MakeGameServer(reg, param1);

    PutScene2GameServerParam put_param;
    put_param.scene_entity_ = scene_entity;
    put_param.server_entity_ = server_entity1;
    PutScene2GameServer(reg, put_param);

    auto& scenes = reg.get<common::Scenes>(e);
    EXPECT_EQ(1, scenes.scenes_.size());
    EXPECT_EQ(1, scenes.scenes_group_[cparam.scene_config_id_].size());

    auto& server_scenes = reg.get<common::SceneIds>(server_entity1);
    EXPECT_EQ(1, server_scenes.size());

    DestroySceneParam dparam;
    dparam.scene_entity_ = scene_entity;
    dparam.scene_map_entity_ = e;
    DestroyScene(reg, dparam);
    EXPECT_TRUE(scenes.scenes_.empty());
    EXPECT_TRUE(scenes.scenes_group_[cparam.scene_config_id_].empty());
    EXPECT_TRUE(server_scenes.empty());
    EXPECT_FALSE(reg.valid(scene_entity));
}

TEST(GameServer, DestroySever)
{
    entt::registry reg;
    auto e = MakeScenes(reg);

    MakeGameServerParam param1;
    param1.server_id_ = 1;

    auto server_entity1 = MakeGameServer(reg, param1);

    MakeGameServerParam param2;
    param2.server_id_ = 2;
    auto server_entity2 = MakeGameServer(reg, param2);

    auto& server_data1 = *reg.get<common::GameServerDataPtr>(server_entity1);
    
    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;

    server1_param.scene_map_entity_ = e;
    server1_param.scene_config_id_ = 3;
    server1_param.server_entity_ = server_entity1;

    server2_param.scene_map_entity_ = e;
    server2_param.scene_config_id_ = 2;
    server2_param.server_entity_ = server_entity2;

    auto scene_id1 =  MakeScene2GameServer(reg, server1_param);
    auto scene_id2 = MakeScene2GameServer(reg, server2_param);

    auto& scenes_id1 = reg.get<common::SceneIds>(server_entity1);

    auto& server_data2 = *reg.get<common::GameServerDataPtr>(server_entity2);

    EXPECT_EQ(1, scenes_id1.size());
    EXPECT_EQ(server_data1.server_id_, param1.server_id_);

    EXPECT_EQ(1, reg.get<common::SceneIds>(server_entity2).size());
    EXPECT_EQ(server_data2.server_id_, param2.server_id_);

    auto& scenes = reg.get<common::Scenes>(e);
    EXPECT_EQ(2, scenes.scenes_.size());

    DestroyServerParam destroy_server_param;
    destroy_server_param.scene_map_entity_ = e;
    destroy_server_param.server_entity_ = server_entity1;
    DestroyServer(reg, destroy_server_param);

    EXPECT_FALSE(reg.valid(server_entity1));
    EXPECT_FALSE(reg.valid(scene_id1));
    EXPECT_TRUE(reg.valid(server_entity2));
    EXPECT_TRUE(reg.valid(scene_id2));

    EXPECT_EQ(1, reg.get<common::SceneIds>(server_entity2).size());
    EXPECT_EQ(1, scenes.scenes_.size());
    EXPECT_EQ(0, scenes.scenes_group_[server1_param.scene_config_id_].size());
    EXPECT_EQ(1, scenes.scenes_group_[server2_param.scene_config_id_].size());

    destroy_server_param.server_entity_ = server_entity2;
    DestroyServer(reg, destroy_server_param);

    EXPECT_EQ(0, scenes.scenes_.size());
    EXPECT_FALSE(reg.valid(server_entity1));
    EXPECT_FALSE(reg.valid(scene_id1));
    EXPECT_FALSE(reg.valid(server_entity2));
    EXPECT_FALSE(reg.valid(scene_id2));

    EXPECT_EQ(0, scenes.scenes_group_[server1_param.scene_config_id_].size());
    EXPECT_EQ(0, scenes.scenes_group_[server2_param.scene_config_id_].size());
}

TEST(GameServer, ServerScene2Sever)
{
    entt::registry reg;
    auto e = MakeScenes(reg);

    MakeGameServerParam cgs1;
    cgs1.server_id_ = 1;

    auto server_entity1 = MakeGameServer(reg, cgs1);

    MakeGameServerParam cgs2;
    cgs2.server_id_ = 2;
    auto server_entity2 = MakeGameServer(reg, cgs2);

   
    auto& server_data2 = *reg.get<common::GameServerDataPtr>(server_entity2);

    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;

    server1_param.scene_map_entity_ = e;
    server1_param.scene_config_id_ = 3;
    server1_param.server_entity_ = server_entity1;

    server2_param.scene_map_entity_ = e;
    server2_param.scene_config_id_ = 2;
    server2_param.server_entity_ = server_entity2;

    auto scene_id1 = MakeScene2GameServer(reg, server1_param);
    auto scene_id2 = MakeScene2GameServer(reg, server2_param);

    auto& server_data1 = *reg.get<common::GameServerDataPtr>(server_entity1);
    auto& scenes_id1 = reg.get<common::SceneIds>(server_entity1);

    EXPECT_EQ(1, scenes_id1.size());
    EXPECT_EQ(server_data1.server_id_, cgs1.server_id_);

    EXPECT_EQ(1, reg.get<common::SceneIds>(server_entity2).size());
    EXPECT_EQ(server_data2.server_id_, cgs2.server_id_);

    auto& scenes = reg.get<common::Scenes>(e);
    EXPECT_EQ(2, scenes.scenes_.size());

    EXPECT_EQ(reg.get<common::GameServerDataPtr>(scene_id1)->server_id_, cgs1.server_id_);
    EXPECT_EQ(reg.get<common::GameServerDataPtr>(scene_id2)->server_id_, cgs2.server_id_);

    MoveServerScene2ServerParam move_scene_param;
    move_scene_param.from_server_entity_ = server_entity1;
    move_scene_param.to_server_entity_ = server_entity2;
    MoveServerScene2Server(reg, move_scene_param);

    EXPECT_TRUE(reg.valid(server_entity1));
    EXPECT_TRUE(reg.valid(scene_id1));
    EXPECT_TRUE(reg.valid(server_entity2));
    EXPECT_TRUE(reg.valid(scene_id2));

    EXPECT_EQ(0, reg.get<common::SceneIds>(server_entity1).size());
    EXPECT_EQ(2, reg.get<common::SceneIds>(server_entity2).size());
    EXPECT_EQ(2, scenes.scenes_.size());
    EXPECT_EQ(1, scenes.scenes_group_[server1_param.scene_config_id_].size());
    EXPECT_EQ(1, scenes.scenes_group_[server2_param.scene_config_id_].size());
    EXPECT_EQ(reg.get<common::GameServerDataPtr>(scene_id1)->server_id_, cgs2.server_id_);
    EXPECT_EQ(reg.get<common::GameServerDataPtr>(scene_id2)->server_id_, cgs2.server_id_);
    EXPECT_EQ(reg.get<common::GameServerDataPtr>(server_entity1).use_count(), 1);
}

TEST(GameServer, PlayerLeaveEnterScene)
{
    entt::registry reg;
    auto e = MakeScenes(reg);

    MakeGameServerParam cgs1;
    cgs1.server_id_ = 1;

    auto server_entity1 = MakeGameServer(reg, cgs1);

    MakeGameServerParam cgs2;
    cgs2.server_id_ = 2;
    auto server_entity2 = MakeGameServer(reg, cgs2);

    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;

    server1_param.scene_map_entity_ = e;
    server1_param.scene_config_id_ = 3;
    server1_param.server_entity_ = server_entity1;

    server2_param.scene_map_entity_ = e;
    server2_param.scene_config_id_ = 2;
    server2_param.server_entity_ = server_entity2;

    auto scene_id1 = MakeScene2GameServer(reg, server1_param);
    auto scene_id2 = MakeScene2GameServer(reg, server2_param);

    EnterSceneParam enter_param1;
    enter_param1.scene_entity_ = scene_id1;
    
    EnterSceneParam enter_param2;
    enter_param2.scene_entity_ = scene_id2;

    uint32_t player_size = 100;
    EntitiesUSet player_entities_set1;
    EntitiesUSet player_entities_set2;
    for (uint32_t i = 0; i < player_size; ++i)
    {
        auto pe = reg.create();
        
        if (i % 2 == 0)
        {
            player_entities_set1.emplace(pe);
            enter_param1.enter_entity_ = pe;
            EnterScene(reg, enter_param1);
        }
        else
        {
            player_entities_set2.emplace(pe);
            enter_param2.enter_entity_ = pe;
            EnterScene(reg, enter_param2);
        }
    }

    auto& scenes_players1 = reg.get<common::PlayerEntities>(scene_id1);
    auto& scenes_players2 = reg.get<common::PlayerEntities>(scene_id2);
    for (auto&it : player_entities_set1)
    {
        EXPECT_TRUE(scenes_players1.find(it) != scenes_players1.end());
        EXPECT_TRUE(reg.get<common::SceneEntityId>(it).scene_entity_ == scene_id1);
    }
    for (auto& it : player_entities_set2)
    {
        EXPECT_TRUE(scenes_players2.find(it) != scenes_players2.end());
        EXPECT_TRUE(reg.get<common::SceneEntityId>(it).scene_entity_ == scene_id2);
    }

    LeaveSceneParam leave_param1;
    LeaveSceneParam leave_param2;

    for (auto& it : player_entities_set1)
    {
        leave_param1.leave_entity_ = it;
        LeaveScene(reg, leave_param1);
        EXPECT_FALSE(scenes_players1.find(it) != scenes_players1.end());
        EXPECT_EQ(reg.try_get<common::SceneEntityId>(it), nullptr);
    }
    for (auto& it : player_entities_set2)
    {
        leave_param2.leave_entity_ = it;
        LeaveScene(reg, leave_param2);
        EXPECT_FALSE(scenes_players2.find(it) != scenes_players2.end());
        EXPECT_EQ(reg.try_get<common::SceneEntityId>(it), nullptr);
    }
    auto& scenes_players11 = reg.get<common::PlayerEntities>(scene_id1);
    auto& scenes_players22 = reg.get<common::PlayerEntities>(scene_id2);
    EXPECT_TRUE(scenes_players11.empty());
    EXPECT_TRUE(scenes_players22.empty());
}

TEST(GameServer, MainTain)
{

}

TEST(GameServer, Update)
{

}

TEST(GameServer, Crash)
{

}

TEST(GameServer, Route)
{
}

TEST(GameServer, WeightRoundRobinMainScene)
{
 /*   entt::registry reg;
    auto e = MakeScenes(reg);

    EntitiesUSet server_entities;
    uint32_t server_size = 10;
    uint32_t per_server_scene = 10;
    MakeGameServerParam cgs1;

    for (uint32_t i = 0; i < server_size; ++i)
    {
        cgs1.server_id_ = i;
        server_entities.emplace(MakeGameServer(reg, cgs1));
    }

    MakeScene2GameServerParam make_server_scene_param;
    make_server_scene_param.scene_map_entity_ = e;
    
    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        make_server_scene_param.scene_config_id_ = i;
        for (auto& it :server_entities)
        {
            make_server_scene_param.server_entity_ = it;
            MakeScene2GameServer(reg, make_server_scene_param);
        }        
    }

    GetWeightRoundRobinSceneParam weight_round_robin_scene;
    weight_round_robin_scene.scene_config_id_ = 0;

   
    uint32_t player_size = 1000;
    EntitiesUSet player_entities_set1;
 
    EnterSceneParam enter_param1;

    for (uint32_t i = 0; i < per_server_scene; ++i)
    {
        auto can_enter = GetWeightRoundRobinSceneEntity(reg, weight_round_robin_scene);
        EXPECT_TRUE(reg.get<common::PlayerEntities>(can_enter).empty());
        auto p_e = reg.create();
        player_entities_set1.emplace(p_e);
        enter_param1.enter_entity_ = p_e;
        enter_param1.scene_entity_ = can_enter;
        EnterScene(reg, enter_param1);
    }   */ 
}

TEST(GameServer, Enter)
{
}

TEST(GameServer, Leave)
{
}

TEST(GameServer, Login)
{
}

TEST(GameServer, Logout)
{
}

TEST(GameServer, Disconnect)
{
}

TEST(GameServer, ReDisconnect)
{
}

TEST(GameServer, CreateDungeon)
{
}

TEST(GameServer, MoveScene)
{
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
