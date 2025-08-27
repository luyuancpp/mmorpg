// test_aoi_system.cpp

#include "scene/scene/system/aoi_system.h"
#include <gtest/gtest.h>
#include "hexagons_grid.h"
#include "core/network/message_system.h"
#include "scene/scene/comp/grid_comp.h"
#include "scene/scene/system/grid_system.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "scene/comp/scene_comp.h"

#include "type_alias/actor.h"
#include "util/random.h"
#include "threading/redis_manager.h"
#include <threading/registry_manager.h>

extern const Point kDefaultSize(20.0, 20.0);
extern const Point kOrigin(0.0, 0.0);
extern const auto kHexLayout = Layout(layout_flat, kDefaultSize, kOrigin);

// Mock class for testing purposes
class MockAoiSystem : public AoiSystem  {
public:
    using AoiSystem::Update;
    using AoiSystem::BeforeLeaveSceneHandler;
};

// Test fixture for AoiSystem
class AoiSystemTest : public ::testing::Test {
protected:
    MockAoiSystem aoi_system;

    void SetUp() override {
        tlsRegistryManager.globalRegistry.emplace<ActorCreateS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.emplace<ActorDestroyS2C>(GlobalEntity());

        tlsRegistryManager.globalRegistry.emplace<ActorListCreateS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.emplace<ActorListDestroyS2C>(GlobalEntity());
    }

    void TearDown() override {
        tlsRegistryManager.globalRegistry.remove<ActorCreateS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.remove<ActorDestroyS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.remove<ActorListCreateS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.remove<ActorListDestroyS2C>(GlobalEntity());

        tlsRegistryManager.sceneRegistry.clear();
        tlsRegistryManager.actorRegistry.clear();
    }
};

// Test GetGridId for Location
TEST_F(AoiSystemTest, TestGetGridIdForLocation) {
    Vector3 location;
    location.set_x(10);
    location.set_y(20);
    auto grid_id = GridSystem::GetGridId(location);

    // Expected value should be calculated based on your logic
    absl::uint128 expected_grid_id = GridSystem::GetGridId(hex_round(pixel_to_hex(kHexLayout, Point(10, 20))));
    EXPECT_EQ(grid_id, expected_grid_id);
}

// Test ScanNeighborGridIds
TEST_F(AoiSystemTest, TestScanNeighborGridIds) {
    Hex hex{ 0, 0, 0 };
    GridSet neighbor_grid_set;
    GridSystem::GetNeighborGridIds(hex, neighbor_grid_set);

    // Check the number of neighbors
    EXPECT_EQ(neighbor_grid_set.size(), 6);

    // Check if specific neighbors are present
    for (int i = 0; i < 6; ++i) {
        EXPECT_NE(neighbor_grid_set.find(GridSystem::GetGridId(hex_neighbor(hex, i))), neighbor_grid_set.end());
    }
}

// Test Update method for player movement
TEST_F(AoiSystemTest, TestUpdatePlayerMovement) {
    // Mock data setup
    auto scene_entity = tlsRegistryManager.sceneRegistry.create();
    auto& scene_grid_list = tlsRegistryManager.sceneRegistry.emplace<SceneGridListComp>(scene_entity);

    SceneEntityComp scene_entity_comp{ scene_entity };

    std::unordered_map<absl::uint128, uint32_t, absl::Hash<absl::uint128>> expected_entity_count;

    for (uint32_t i = 0; i < 10; ++i) {
        auto player_entity = tlsRegistryManager.actorRegistry.create();

        Transform& transform = tlsRegistryManager.actorRegistry.emplace<Transform>(player_entity);
        transform.mutable_location()->set_x(tlsRandom.RandReal<double>(0, 1000));
        transform.mutable_location()->set_y(tlsRandom.RandReal<double>(0, 1000));

        tlsRegistryManager.actorRegistry.emplace<SceneEntityComp>(player_entity, scene_entity_comp);

        // Invoke Update method
        aoi_system.Update(0.1);

        auto grid_id = GridSystem::GetGridId(transform.location());
        ++expected_entity_count[grid_id];
        EXPECT_TRUE(scene_grid_list[grid_id].entities.contains(player_entity));
        EXPECT_EQ(scene_grid_list[grid_id].entities.size(), expected_entity_count[grid_id]);
    }

    for (auto&& [scene, grid_list] : tlsRegistryManager.sceneRegistry.view<SceneGridListComp>().each()) {
        for (const auto& [_, entity_list] : grid_list) {
            EXPECT_FALSE(entity_list.entities.empty());
        }
    }

    GridSystem::UpdateLogGridSize(0.1);
}

// Test player movement across six neighboring hexes
TEST_F(AoiSystemTest, TestPlayerMovementAcrossSixHexes) {
    // Mock data setup
    auto scene_entity = tlsRegistryManager.sceneRegistry.create();
    auto& scene_grid_list = tlsRegistryManager.sceneRegistry.emplace<SceneGridListComp>(scene_entity);

    auto player_entity = tlsRegistryManager.actorRegistry.create();

    Transform& transform = tlsRegistryManager.actorRegistry.emplace<Transform>(player_entity);
    transform.mutable_location()->set_x(0);
    transform.mutable_location()->set_y(0);

    SceneEntityComp scene_entity_comp{ scene_entity };
    tlsRegistryManager.actorRegistry.emplace<SceneEntityComp>(player_entity, scene_entity_comp);

    // Initial position
    aoi_system.Update(0.1);
    Hex initial_hex = hex_round(pixel_to_hex(kHexLayout, Point(0, 0)));
    auto initial_grid_id = GridSystem::GetGridId(initial_hex);
    EXPECT_TRUE(scene_grid_list[initial_grid_id].entities.contains(player_entity));

    // Move the player to each neighboring hex
    for (int i = 0; i < 6; ++i) {
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
    for (auto&& [scene, grid_list] : tlsRegistryManager.sceneRegistry.view<SceneGridListComp>().each()) {
        for (const auto& [_, entity_list] : grid_list) {
            if (entity_list.entities.empty()) {
                continue;
            }
            EXPECT_EQ(entity_list.entities.size(), 1);
            expected_size += entity_list.entities.size();
        }
    }

    EXPECT_EQ(expected_size, 1);
}



// Mocking necessary classes and functions for testing
class MockViewSystem {
public:
    static bool CheckSendNpcEnterMessage(entt::entity observer, entt::entity entity) {
        return true; // Mock behavior
    }

    static bool CheckSendPlayerEnterMessage(entt::entity observer, entt::entity entity) {
        return true; // Mock behavior
    }

    static void FillActorCreateS2CInfo(entt::entity entity) {
        // Mock behavior
    }
};

EntityUnorderedMap entitiesToNotifyEntry;
EntityUnorderedMap entitiesToNotifyExit;

// Test fixture class
class AoiSystemTest1 : public ::testing::Test {
protected:
    AoiSystem aoiSystem;
    entt::entity entity1;
    entt::entity entity2;
    SceneEntityComp sceneEntityComp1;
    SceneEntityComp sceneEntityComp2;


    void SetUp() override {

        tlsRegistryManager.globalRegistry.emplace<ActorCreateS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.emplace<ActorDestroyS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.emplace<ActorListCreateS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.emplace<ActorListDestroyS2C>(GlobalEntity());
        
        // Setup mock data
        entity1 = tlsRegistryManager.actorRegistry.create();
        entity2 = tlsRegistryManager.actorRegistry.create();

        // Set up mock components
        auto sceneEntity = tlsRegistryManager.sceneRegistry.create();
        sceneEntityComp1.sceneEntity = sceneEntity;
        sceneEntityComp2.sceneEntity = sceneEntity;
        tlsRegistryManager.actorRegistry.emplace<SceneEntityComp>(entity1, sceneEntityComp1);
        tlsRegistryManager.actorRegistry.emplace<SceneEntityComp>(entity2, sceneEntityComp2);

        // Set initial positions
        auto& transform1 = tlsRegistryManager.actorRegistry.emplace<Transform>(entity1);

        auto& transform2 = tlsRegistryManager.actorRegistry.emplace<Transform>(entity2);
        transform2.mutable_location()->set_x(100);
        transform2.mutable_location()->set_y(100);


        // Set up grid list
        tlsRegistryManager.sceneRegistry.emplace<SceneGridListComp>(sceneEntityComp1.sceneEntity);
    }

    void TearDown() override {
        tlsRegistryManager.globalRegistry.remove<ActorCreateS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.remove<ActorDestroyS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.remove<ActorListCreateS2C>(GlobalEntity());
        tlsRegistryManager.globalRegistry.remove<ActorListDestroyS2C>(GlobalEntity());

        tlsRegistryManager.actorRegistry.clear();
        tlsRegistryManager.sceneRegistry.clear();

        entitiesToNotifyEntry.clear();
        entitiesToNotifyExit.clear();
    }
};

// Test case for entering the view
TEST_F(AoiSystemTest1, TestEntityEnterView) {
    // Move entity2 to be within view range of entity1
    auto& location = *tlsRegistryManager.actorRegistry.get<Transform>(entity2).mutable_location();
    location.set_x(20);
    location.set_y(20);

    aoiSystem.Update(0.0);

    // Check that entity1 should be notified of entity2 entering its view
    // Add your assertions here
    // For example:
    EXPECT_TRUE(entitiesToNotifyEntry.find(entity2)->second == entity1);
    EXPECT_TRUE(entitiesToNotifyEntry.find(entity1)->second == entity2);
}

// Test case for leaving the view
TEST_F(AoiSystemTest1, TestEntityLeaveView) {
    // Move entity2 out of view range of entity1
    auto& location = *tlsRegistryManager.actorRegistry.get<Transform>(entity2).mutable_location();
    location.set_x(0);
    location.set_y(0);

    aoiSystem.Update(0.0);

    EXPECT_TRUE(entitiesToNotifyEntry.find(entity2)->second == entity1);
    EXPECT_TRUE(entitiesToNotifyEntry.find(entity1)->second == entity2);

    location.set_x(50);
    location.set_y(50);

    aoiSystem.Update(0.0);

    // Check that entity1 should be notified of entity2 leaving its view
    // Add your assertions here
    // For example:
    EXPECT_TRUE(entitiesToNotifyExit.find(entity2)->second == entity1);
    EXPECT_TRUE(entitiesToNotifyExit.find(entity1)->second == entity2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
