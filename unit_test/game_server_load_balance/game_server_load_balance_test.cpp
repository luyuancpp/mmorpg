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
    auto e = MakeMainSceneMap(reg);
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

TEST(GameServer, PutSceneId2Sever)
{
    entt::registry reg;
    auto e = MakeMainSceneMap(reg);
    MakeSceneParam cparam;
    cparam.scene_map_entity_ = e;
    MakeMainScene(reg, cparam);
    MakeGameServerParam param;
    param.scene_map_entity_ = e;
    param.server_id_ = 1;
    param.scenes_id_ = reg.get<common::Scenes>(e).scenes_;
    auto se = MakeGameServer(reg, param);   
    auto& server_data = reg.get<common::GameServerData>(se);
    auto& scenes_id = reg.get<common::SceneIds>(se);
    EXPECT_EQ(1, scenes_id.size());
    EXPECT_EQ(server_data.server_id_, param.server_id_);
}

TEST(GameServer, CreateScene2Sever)
{
    entt::registry reg;
    auto e = MakeMainSceneMap(reg);

    MakeGameServerParam param1;
    param1.scene_map_entity_ = e;
    param1.server_id_ = 1;

    MakeSceneParam cparam;
    cparam.scene_map_entity_ = e;

    for (uint32_t i = 0; i < scene_config_size; ++i)
    {
        MakeMainScene(reg, cparam);
        param1.scenes_id_.emplace(reg.create());
    }
    auto se1 = MakeGameServer(reg, param1);

    MakeGameServerParam param2;
    param2.scene_map_entity_ = e;
    param2.server_id_ = 1;
    auto se2 = MakeGameServer(reg, param2);

    auto& server_data1 = reg.get<common::GameServerData>(se1);
    auto& scenes_id1 = reg.get<common::SceneIds>(se1);
 
    auto& server_data2 = reg.get<common::GameServerData>(se2);
    auto& scenes_id2 = reg.get<common::SceneIds>(se2);

    MakeScene2GameServerParam server1_param;
    MakeScene2GameServerParam server2_param;

    server1_param.scene_map_entity_ = e;
    server1_param.scene_config_id_ = 2;
    server1_param.server_entity_ = se1;

    server2_param.scene_map_entity_ = e;
    server2_param.scene_config_id_ = 2;
    server2_param.server_entity_ = se2;

    MakeScene2GameServer(reg, server1_param);
    MakeScene2GameServer(reg, server2_param);

    EXPECT_EQ(std::size_t(scene_config_size + 1), scenes_id1.size());
    EXPECT_EQ(server_data1.server_id_, param1.server_id_);

    EXPECT_EQ(1, scenes_id2.size());
    EXPECT_EQ(server2_param.scene_config_id_, reg.get<common::SceneConfigId>(*scenes_id2.begin()).scene_config_id_);
    EXPECT_EQ(server_data2.server_id_, param2.server_id_);

    std::size_t total_scene_size = scene_config_size + 2;
    auto& scenes = reg.get<common::Scenes>(e);
    EXPECT_EQ(total_scene_size, scenes.scenes_.size());
}

TEST(GameServer, RemoveScene2Sever)
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
