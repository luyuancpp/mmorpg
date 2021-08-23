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
    auto& c = reg.get<common::SceneMap>(e);
    MakeMainSceneParam param;
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
    auto& c = reg.get<common::SceneMap>(e);
    for (uint32_t i = 0; i < scene_config_size; ++i)
    {       
        EXPECT_EQ(c.scenes_group_[i].size(), std::size_t(per_scene_config_size));
    }
    EXPECT_EQ(c.scenes_.size(), std::size_t(scene_config_size * per_scene_config_size));
}

TEST(GameServer, PutScene2Sever)
{
    entt::registry reg;
    auto e = TestCreateMainScene(reg);
}

TEST(GameServer, MoveScene2Sever)
{

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
