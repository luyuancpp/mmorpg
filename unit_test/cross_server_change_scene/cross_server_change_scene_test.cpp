#include <gtest/gtest.h>

#include <boost/circular_buffer.hpp>

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/tips_id.h"
#include "src/system/player_change_scene.h"

entt::entity CreatePlayer()
{
	auto player = registry.create();
	PlayerChangeSceneSystem::InitChangeSceneQueue(player);
    return player;
}

TEST(PlayerChangeScene, QueueFull)
{
    auto player = CreatePlayer();
    for (uint8_t i = 0; i < PlayerMsChangeSceneQueue::kMaxChangeSceneQueue; ++i)
    {
        MsChangeSceneInfo info;
        EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::ChangeScene(player, std::move(info)));
    }
    MsChangeSceneInfo info;
    EXPECT_EQ(kRetChangeScenePlayerQueueCompnentFull, PlayerChangeSceneSystem::ChangeScene(player, std::move(info)));
}

//1:同一个gs直接切，队列直接成功
TEST(PlayerChangeScene, ChangeSameGsSceneNotEnqueue)
{
    auto player = CreatePlayer();
    uint32_t gs_node_id = 1;
    MsChangeSceneInfo change_info;
    change_info.set_gs_node_id(gs_node_id);
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::ChangeScene(player, std::move(change_info)));
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
