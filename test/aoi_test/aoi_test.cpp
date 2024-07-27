// test_aoi_system.cpp

#include <gtest/gtest.h>
#include "system/scene/aoi.h"
#include "comp/scene.h"
#include "comp/scene/grid.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "proto/logic/event/scene_event.pb.h"
#include "muduo/base/Logging.h"
#include "system/network/message_system.h"
#include "service/scene_client_player_service.h"
#include "hexagons_grid.h"
#include "thread_local/storage.h"
#include "type_alias/actor.h"
#include "util/random.h"

// Create a mock class for testing purposes
class MockAoiSystem : public AoiSystem {
public:
    using AoiSystem::GetGridId;
    using AoiSystem::ScanNeighborGridId;
    using AoiSystem::HandlePlayerMovement;
};

// Define the test fixture
class AoiSystemTest : public ::testing::Test {
protected:
    MockAoiSystem aoi_system;

    void SetUp() override {
        tls.globalRegistry.emplace<ActorCreateS2C>(global_entity());
        tls.globalRegistry.emplace<ActorDestroyS2C>(global_entity());
    }

    void TearDown() override {
        tls.globalRegistry.remove<ActorCreateS2C>(global_entity());
        tls.globalRegistry.remove<ActorDestroyS2C>(global_entity());
        tls.sceneRegistry.clear();
        tls.registry.clear();
    }
};

// Test GetGridId for Location
TEST_F(AoiSystemTest, TestGetGridIdForLocation) {
    Location loc;
    loc.set_x(10);
    loc.set_y(20);
    auto grid_id = aoi_system.GetGridId(loc);
    // Assuming some expected value based on the grid_id calculation
    absl::uint128 expected_grid_id = 1/* Expected value */;
    EXPECT_EQ(grid_id, expected_grid_id);
}

// Test ScanNeighborGridId
TEST_F(AoiSystemTest, TestScanNeighborGridId) {
    Hex hex{ 0, 0, 0 };
    GridSet grid_set;
    aoi_system.ScanNeighborGridId(hex, grid_set);

    // Check the number of neighbors
    EXPECT_EQ(grid_set.size(), 6);

    // Check if specific neighbors are present
    EXPECT_NE(grid_set.find(aoi_system.GetGridId(hex_neighbor(hex, 0))), grid_set.end());
    EXPECT_NE(grid_set.find(aoi_system.GetGridId(hex_neighbor(hex, 1))), grid_set.end());
    EXPECT_NE(grid_set.find(aoi_system.GetGridId(hex_neighbor(hex, 2))), grid_set.end());
}

// Test HandlePlayerMovement
TEST_F(AoiSystemTest, TestHandlePlayerMovement) {
    // Setup mock data
    auto sceneEntity = tls.sceneRegistry.create();
    auto& sceneGridList = tls.sceneRegistry.emplace<SceneGridList>(sceneEntity);

    std::unordered_map<absl::uint128, uint32_t, absl::Hash<absl::uint128>> exList;

    for (uint32_t i = 0; i < 100; ++i)
    {
        entt::entity playerEntity = tls.registry.create();/* Create or mock a player entity */;

        Transform& transform = tls.registry.emplace<Transform>(playerEntity);
        transform.mutable_location()->set_x(tls_rand.RandDobule(0, 10000));
        transform.mutable_location()->set_y(tls_rand.RandDobule(0, 10000));

        SceneEntityComp sceneEntityComp{ sceneEntity };

        // Call the method to test
        aoi_system.HandlePlayerMovement(playerEntity, transform, sceneEntityComp);

        auto grid_id = aoi_system.GetGridId(transform.location());
        ++exList[grid_id];
        EXPECT_TRUE(sceneGridList[grid_id].entity_list.contains(playerEntity));
        EXPECT_EQ(sceneGridList[grid_id].entity_list.size(), exList[grid_id]);
    }
}

// Add more tests as needed

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
