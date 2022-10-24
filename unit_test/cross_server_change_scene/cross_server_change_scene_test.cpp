#include <gtest/gtest.h>

#include <boost/circular_buffer.hpp>

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/tips_id.h"
#include "src/system/player_change_scene.h"


TEST(PlayerChangeScene, QueueFull)
{
    auto player = registry.create();
    PlayerChangeSceneSystem::InitChangeSceneQueue(player);
    for (uint8_t i = 0; i < PlayerMsChangeSceneQueue::kMaxChangeSceneQueue; ++i)
    {
        MsChangeSceneInfo info;
        EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::ChangeScene(player, std::move(info)));
    }
    MsChangeSceneInfo info;
    EXPECT_EQ(kRetChangeScenePlayerQueueCompnentFull, PlayerChangeSceneSystem::ChangeScene(player, std::move(info)));
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
