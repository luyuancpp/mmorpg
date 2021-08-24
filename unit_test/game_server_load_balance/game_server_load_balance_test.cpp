#include <gtest/gtest.h>

#include "src/factories/scene_factories.hpp"
#include "src/game/game_client.h"
#include "src/game_logic/comp/server_list.hpp"
#include "src/game_logic/game_registry.h"
#include "src/sys/scene_sys.hpp"

using namespace  master;
using namespace  common;

uint32_t scene_config_size = 50;
uint32_t per_scene_config_size = 10;

entt::entity TestCreateMainScene(entt::registry& reg)
{
    auto e = MakeScenes(reg);
    MakeSceneParam param;
    param.scene_map_entity_ = e;
    for (uint32_t i = 0; i < scene_config_size; ++i)
    {
        param.scene_config_id_ = i;
        for (uint32_t j = 0; j < per_scene_config_size; ++j)
        {
            MakeMainScene(reg, param);
        }
    }
    return e;
}

TEST(GameServer, CreateMainScene)
{
    entt::registry reg;
    auto e = TestCreateMainScene(reg);
    auto& c = reg.get<common::Scenes>(e);
    for (uint32_t i = 0; i < scene_config_size; ++i)
    {       
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

    auto& server_data1 = *reg.get<common::GameServerDataPtr>(server_entity1);
    auto& scenes_id1 = reg.get<common::SceneIds>(server_entity1);
 
    auto& server_data2 = *reg.get<common::GameServerDataPtr>(server_entity2);
    auto& scenes_id2 = reg.get<common::SceneIds>(server_entity2);

    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;

    server1_param.scene_map_entity_ = e;
    server1_param.scene_config_id_ = 2;
    server1_param.server_entity_ = server_entity1;

    server2_param.scene_map_entity_ = e;
    server2_param.scene_config_id_ = 2;
    server2_param.server_entity_ = server_entity2;

    MakeScene2GameServer(reg, server1_param);
    MakeScene2GameServer(reg, server2_param);

    EXPECT_EQ(1, scenes_id1.size());
    EXPECT_EQ(server_data1.server_id_, param1.server_id_);

    EXPECT_EQ(1, scenes_id2.size());
    EXPECT_EQ(server2_param.scene_config_id_, reg.get<common::SceneConfigId>(*scenes_id2.begin()).scene_config_id_);
    EXPECT_EQ(server_data2.server_id_, param2.server_id_);

    auto& scenes = reg.get<common::Scenes>(e);
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

}

TEST(GameServer, ServerScene2Sever)
{

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

TEST(GameServer, WeightRoundRobin)
{
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
