#include <gtest/gtest.h>

#include <boost/circular_buffer.hpp>

#include "muduo/base/Timestamp.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/tips_id.h"
#include "src/system/player_change_scene.h"
#include "src/game_logic/scene/scene.h"

ScenesSystem sm;
EntityVector scene_list;

entt::entity CreatePlayer()
{
	auto player = registry.create();
	PlayerChangeSceneSystem::InitChangeSceneQueue(player);
    return player;
}

TEST(PlayerChangeScene, CreateMainScene)
{
    CreateSceneP param;
    for (uint32_t i = 0; i < 10; ++i)
    {
        param.scene_confid_ = i;
        for (uint32_t j = 0; j < 2; ++j)
        {
            scene_list.push_back(sm.CreateScene(param));
        }
    }
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

//1:ͬһ��gsֱ���У�����ֱ�ӳɹ�
TEST(PlayerChangeScene, ChangeSameGsSceneNotEnqueue)
{
    auto player = CreatePlayer();
    auto from_gs = registry.create();
    auto to_gs = registry.create();
    auto scene_id = registry.get<SceneInfo>(*(scene_list.begin()++)).scene_id();
    EnterSceneParam ep;
    ep.enterer_ = player;
    auto from_scene = ScenesSystem::get_scene(scene_id);
    ep.scene_ = from_scene;
    ScenesSystem::EnterScene(ep);
    MsChangeSceneInfo change_info;
    change_info.mutable_scene_info()->set_scene_id(scene_id);
    change_info.set_change_gs_type(MsChangeSceneInfo::eSameGs);//todo scene logic
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::ChangeScene(player, std::move(change_info)));    
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(registry.get<PlayerMsChangeSceneQueue>(player).change_scene_queue_.empty());
}

TEST(PlayerChangeScene, Gs1SceneToGs2SceneInMyServer)
{
    auto player = CreatePlayer();
    auto from_gs = registry.create();
    auto to_gs = registry.create();
    auto scene_id = registry.get<SceneInfo>(*(scene_list.begin()++)).scene_id();
    EnterSceneParam ep;
    ep.enterer_ = player;
    auto from_scene = ScenesSystem::get_scene(scene_id);
    ep.scene_ = from_scene;
    ScenesSystem::EnterScene(ep);
    MsChangeSceneInfo change_info;
    change_info.mutable_scene_info()->set_scene_id(scene_id);
    change_info.set_change_gs_type(MsChangeSceneInfo::eDifferentGs);//todo scene logic
    change_info.set_change_gs_status(MsChangeSceneInfo::eLeaveGsScene);
    EXPECT_EQ(kRetOK, PlayerChangeSceneSystem::ChangeScene(player, std::move(change_info)));
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
    EXPECT_TRUE(!registry.get<PlayerMsChangeSceneQueue>(player).change_scene_queue_.empty());
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
