// aoi_system_test.cpp — AOI（兴趣区域）系统测试

#include "spatial/system/aoi.h"
#include <gtest/gtest.h>
#include "hexagons_grid.h"
#include "core/network/message_system.h"
#include "spatial/comp/grid_comp.h"
#include "spatial/system/grid.h"
#include "proto/common/component/actor_comp.pb.h"
#include "modules/scene/comp/scene_comp.h"

#include "type_alias/actor.h"
#include "engine/core/utils/random/random.h"
#include "thread_context/redis_manager.h"
#include <thread_context/registry_manager.h>

extern const Point kDefaultSize(20.0, 20.0);
extern const Point kOrigin(0.0, 0.0);
extern const auto kHexLayout = Layout(layout_flat, kDefaultSize, kOrigin);

// ---------------------------------------------------------------------------
// 基础测试夹具：使用 MockAoiSystem 暴露内部方法
// ---------------------------------------------------------------------------

class MockAoiSystem : public AoiSystem
{
public:
    using AoiSystem::BeforeLeaveSceneHandler;
    using AoiSystem::Update;
};

// ---------------------------------------------------------------------------
// AoiSystemTest —— 网格分配、邻居查找、玩家移动
// ---------------------------------------------------------------------------

class AoiSystemTest : public ::testing::Test
{
protected:
    MockAoiSystem aoi_system;

    void SetUp() override
    {
        tlsEcs.globalRegistry.emplace<ActorCreateS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.emplace<ActorDestroyS2C>(tlsEcs.GlobalEntity());

        tlsEcs.globalRegistry.emplace<ActorListCreateS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.emplace<ActorListDestroyS2C>(tlsEcs.GlobalEntity());
    }

    void TearDown() override
    {
        tlsEcs.globalRegistry.remove<ActorCreateS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.remove<ActorDestroyS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.remove<ActorListCreateS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.remove<ActorListDestroyS2C>(tlsEcs.GlobalEntity());

        tlsEcs.sceneRegistry.clear();
        tlsEcs.actorRegistry.clear();
    }
};

// 坐标→网格 ID
TEST_F(AoiSystemTest, TestGetGridIdForLocation)
{
    Vector3 location;
    location.set_x(10);
    location.set_y(20);
    auto grid_id = GridSystem::GetGridId(location);

    // Expected value should be calculated based on your logic
    absl::uint128 expected_grid_id = GridSystem::GetGridId(hex_round(pixel_to_hex(kHexLayout, Point(10, 20))));
    EXPECT_EQ(grid_id, expected_grid_id);
}

// 扫描邻居网格（六边形 6 个邻居）
TEST_F(AoiSystemTest, TestScanNeighborGridIds)
{
    Hex hex{0, 0, 0};
    GridSet neighbor_grid_set;
    GridSystem::GetNeighborGridIds(hex, neighbor_grid_set);

    // Check the number of neighbors
    EXPECT_EQ(neighbor_grid_set.size(), 6);

    // Check if specific neighbors are present
    for (int i = 0; i < 6; ++i)
    {
        EXPECT_NE(neighbor_grid_set.find(GridSystem::GetGridId(hex_neighbor(hex, i))), neighbor_grid_set.end());
    }
}

// 玩家随机移动后网格分配正确性
TEST_F(AoiSystemTest, TestUpdatePlayerMovement)
{
    // Mock data setup
    auto scene_entity = tlsEcs.sceneRegistry.create();
    auto &scene_grid_list = tlsEcs.sceneRegistry.get_or_emplace<SceneGridListComp>(scene_entity);

    SceneEntityComp scene_entity_comp{scene_entity};

    std::unordered_map<absl::uint128, uint32_t, absl::Hash<absl::uint128>> expected_entity_count;

    for (uint32_t i = 0; i < 10; ++i)
    {
        auto player_entity = tlsEcs.actorRegistry.create();

        Transform &transform = tlsEcs.actorRegistry.get_or_emplace<Transform>(player_entity);
        transform.mutable_location()->set_x(tlsRandom.RandReal<double>(0, 1000));
        transform.mutable_location()->set_y(tlsRandom.RandReal<double>(0, 1000));

        tlsEcs.actorRegistry.emplace<SceneEntityComp>(player_entity, scene_entity_comp);

        // Invoke Update method
        aoi_system.Update(0.1);

        auto grid_id = GridSystem::GetGridId(transform.location());
        ++expected_entity_count[grid_id];
        EXPECT_TRUE(scene_grid_list[grid_id].entities.contains(player_entity));
        EXPECT_EQ(scene_grid_list[grid_id].entities.size(), expected_entity_count[grid_id]);
    }

    for (auto &&[scene, grid_list] : tlsEcs.sceneRegistry.view<SceneGridListComp>().each())
    {
        for (const auto &[_, entity_list] : grid_list)
        {
            EXPECT_FALSE(entity_list.entities.empty());
        }
    }

    GridSystem::UpdateLogGridSize();
}

// 玩家在六个相邻六边形之间往返移动
TEST_F(AoiSystemTest, TestPlayerMovementAcrossSixHexes)
{
    // Mock data setup
    auto scene_entity = tlsEcs.sceneRegistry.create();
    auto &scene_grid_list = tlsEcs.sceneRegistry.emplace<SceneGridListComp>(scene_entity);

    auto player_entity = tlsEcs.actorRegistry.create();

    Transform &transform = tlsEcs.actorRegistry.emplace<Transform>(player_entity);
    transform.mutable_location()->set_x(0);
    transform.mutable_location()->set_y(0);

    SceneEntityComp scene_entity_comp{scene_entity};
    tlsEcs.actorRegistry.emplace<SceneEntityComp>(player_entity, scene_entity_comp);

    // Initial position
    aoi_system.Update(0.1);
    Hex initial_hex = hex_round(pixel_to_hex(kHexLayout, Point(0, 0)));
    auto initial_grid_id = GridSystem::GetGridId(initial_hex);
    EXPECT_TRUE(scene_grid_list[initial_grid_id].entities.contains(player_entity));

    // Move the player to each neighboring hex
    for (int i = 0; i < 6; ++i)
    {
        Hex neighbor_hex = hex_neighbor(initial_hex, i);
        Point neighbor_point = hex_to_pixel(kHexLayout, neighbor_hex);
        transform.mutable_location()->set_x(neighbor_point.x);
        transform.mutable_location()->set_y(neighbor_point.y);

        // Update the system
        aoi_system.Update(0.1);

        auto new_grid_id = GridSystem::GetGridId(neighbor_hex);
        EXPECT_TRUE(scene_grid_list[new_grid_id].entities.contains(player_entity));
        EXPECT_FALSE(scene_grid_list[initial_grid_id].entities.contains(player_entity));

        // Move back to initial hex for next iteration
        transform.mutable_location()->set_x(0);
        transform.mutable_location()->set_y(0);
        aoi_system.Update(0.1);
        EXPECT_TRUE(scene_grid_list[initial_grid_id].entities.contains(player_entity));
        EXPECT_FALSE(scene_grid_list[new_grid_id].entities.contains(player_entity));
    }

    std::size_t expected_size = 0;
    for (auto &&[scene, grid_list] : tlsEcs.sceneRegistry.view<SceneGridListComp>().each())
    {
        for (const auto &[_, entity_list] : grid_list)
        {
            if (entity_list.entities.empty())
            {
                continue;
            }
            EXPECT_EQ(entity_list.entities.size(), 1);
            expected_size += entity_list.entities.size();
        }
    }

    EXPECT_EQ(expected_size, 1);
}

// ---------------------------------------------------------------------------
// AoiEntityVisibilityTest —— 实体进入/离开视野通知
// ---------------------------------------------------------------------------

EntityUnorderedMap entitiesToNotifyEntry;
EntityUnorderedMap entitiesToNotifyExit;

class AoiEntityVisibilityTest : public ::testing::Test
{
protected:
    AoiSystem aoiSystem;
    entt::entity entity1;
    entt::entity entity2;
    SceneEntityComp sceneEntityComp1;
    SceneEntityComp sceneEntityComp2;

    void SetUp() override
    {
        tlsEcs.globalRegistry.emplace<ActorCreateS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.emplace<ActorDestroyS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.emplace<ActorListCreateS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.emplace<ActorListDestroyS2C>(tlsEcs.GlobalEntity());

        entity1 = tlsEcs.actorRegistry.create();
        entity2 = tlsEcs.actorRegistry.create();

        // 将两个实体放入同一场景
        auto sceneEntity = tlsEcs.sceneRegistry.create();
        sceneEntityComp1.sceneEntity = sceneEntity;
        sceneEntityComp2.sceneEntity = sceneEntity;
        tlsEcs.actorRegistry.emplace<SceneEntityComp>(entity1, sceneEntityComp1);
        tlsEcs.actorRegistry.emplace<SceneEntityComp>(entity2, sceneEntityComp2);

        // entity1 在原点，entity2 在 (100,100)
        auto &transform1 = tlsEcs.actorRegistry.emplace<Transform>(entity1);

        auto &transform2 = tlsEcs.actorRegistry.emplace<Transform>(entity2);
        transform2.mutable_location()->set_x(100);
        transform2.mutable_location()->set_y(100);

        // 创建场景网格
        tlsEcs.sceneRegistry.emplace<SceneGridListComp>(sceneEntityComp1.sceneEntity);
    }

    void TearDown() override
    {
        tlsEcs.globalRegistry.remove<ActorCreateS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.remove<ActorDestroyS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.remove<ActorListCreateS2C>(tlsEcs.GlobalEntity());
        tlsEcs.globalRegistry.remove<ActorListDestroyS2C>(tlsEcs.GlobalEntity());

        tlsEcs.actorRegistry.clear();
        tlsEcs.sceneRegistry.clear();

        entitiesToNotifyEntry.clear();
        entitiesToNotifyExit.clear();
    }
};

// 实体进入视野
TEST_F(AoiEntityVisibilityTest, TestEntityEnterView)
{
    // 将 entity2 移到 entity1 附近
    auto &location = *tlsEcs.actorRegistry.get_or_emplace<Transform>(entity2).mutable_location();
    location.set_x(20);
    location.set_y(20);

    aoiSystem.Update(0.0);

    // 双方都应收到进入通知
    auto it2 = entitiesToNotifyEntry.find(entity2);
    ASSERT_NE(it2, entitiesToNotifyEntry.end()) << "entity2 not found in entitiesToNotifyEntry";
    EXPECT_EQ(it2->second, entity1);

    auto it1 = entitiesToNotifyEntry.find(entity1);
    ASSERT_NE(it1, entitiesToNotifyEntry.end()) << "entity1 not found in entitiesToNotifyEntry";
    EXPECT_EQ(it1->second, entity2);
}

// 实体离开视野
TEST_F(AoiEntityVisibilityTest, TestEntityLeaveView)
{
    // 先将 entity2 移到 entity1 附近触发进入
    auto &location = *tlsEcs.actorRegistry.get_or_emplace<Transform>(entity2).mutable_location();
    location.set_x(0);
    location.set_y(0);

    aoiSystem.Update(0.0);

    {
        auto it2 = entitiesToNotifyEntry.find(entity2);
        ASSERT_NE(it2, entitiesToNotifyEntry.end()) << "entity2 not found in entitiesToNotifyEntry (enter)";
        EXPECT_EQ(it2->second, entity1);

        auto it1 = entitiesToNotifyEntry.find(entity1);
        ASSERT_NE(it1, entitiesToNotifyEntry.end()) << "entity1 not found in entitiesToNotifyEntry (enter)";
        EXPECT_EQ(it1->second, entity2);
    }

    // 然后将 entity2 移远，验证离开通知
    location.set_x(50);
    location.set_y(50);

    aoiSystem.Update(0.0);

    {
        auto it2 = entitiesToNotifyExit.find(entity2);
        ASSERT_NE(it2, entitiesToNotifyExit.end()) << "entity2 not found in entitiesToNotifyExit (leave)";
        EXPECT_EQ(it2->second, entity1);

        auto it1 = entitiesToNotifyExit.find(entity1);
        ASSERT_NE(it1, entitiesToNotifyExit.end()) << "entity1 not found in entitiesToNotifyExit (leave)";
        EXPECT_EQ(it1->second, entity2);
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
