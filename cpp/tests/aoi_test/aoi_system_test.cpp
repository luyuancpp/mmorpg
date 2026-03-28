// aoi_system_test.cpp — AOI (Area of Interest) system tests
//
// Verifies:
//   1. Hex-grid coordinate mapping and neighbor lookup
//   2. Entity-to-grid assignment after random movement
//   3. Round-trip movement across all six hex neighbors
//   4. Entity enter/leave visibility via AoiListComp

#include "spatial/system/aoi.h"
#include <gtest/gtest.h>
#include "hexagons_grid.h"
#include "core/network/message_system.h"
#include "spatial/comp/grid_comp.h"
#include "spatial/comp/scene_node_scene_comp.h"
#include "spatial/system/grid.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/event/scene_event.pb.h"
#include "modules/scene/comp/scene_comp.h"

#include "type_alias/actor.h"
#include "engine/core/utils/random/random.h"
#include "thread_context/redis_manager.h"
#include <thread_context/registry_manager.h>

extern const Point kDefaultSize(20.0, 20.0);
extern const Point kOrigin(0.0, 0.0);
extern const auto kHexLayout = Layout(layout_flat, kDefaultSize, kOrigin);

// ---------------------------------------------------------------------------
// Helper: register / unregister global message components used by AoiSystem
// ---------------------------------------------------------------------------

static void RegisterGlobalMessageComponents()
{
    tlsEcs.globalRegistry.emplace<ActorCreateS2C>(tlsEcs.GlobalEntity());
    tlsEcs.globalRegistry.emplace<ActorDestroyS2C>(tlsEcs.GlobalEntity());
    tlsEcs.globalRegistry.emplace<ActorListCreateS2C>(tlsEcs.GlobalEntity());
    tlsEcs.globalRegistry.emplace<ActorListDestroyS2C>(tlsEcs.GlobalEntity());
}

static void UnregisterGlobalMessageComponents()
{
    tlsEcs.globalRegistry.remove<ActorCreateS2C>(tlsEcs.GlobalEntity());
    tlsEcs.globalRegistry.remove<ActorDestroyS2C>(tlsEcs.GlobalEntity());
    tlsEcs.globalRegistry.remove<ActorListCreateS2C>(tlsEcs.GlobalEntity());
    tlsEcs.globalRegistry.remove<ActorListDestroyS2C>(tlsEcs.GlobalEntity());
}

// ---------------------------------------------------------------------------
// Fixture: Grid assignment, neighbor lookup, player movement
// ---------------------------------------------------------------------------

class AoiGridTest : public ::testing::Test
{
protected:
    void SetUp() override { RegisterGlobalMessageComponents(); }

    void TearDown() override
    {
        UnregisterGlobalMessageComponents();
        tlsEcs.sceneRegistry.clear();
        tlsEcs.actorRegistry.clear();
    }
};

// Verify that a world-space coordinate maps to the expected hex grid id.
TEST_F(AoiGridTest, CoordinateMapsToCorrectGridId)
{
    Vector3 location;
    location.set_x(10);
    location.set_y(20);

    const auto gridId = GridSystem::GetGridId(location);
    const auto expectedGridId = GridSystem::GetGridId(hex_round(pixel_to_hex(kHexLayout, Point(10, 20))));
    EXPECT_EQ(gridId, expectedGridId);
}

// A hex cell must have exactly 6 neighbors, each matching hex_neighbor().
TEST_F(AoiGridTest, HexHasSixCorrectNeighbors)
{
    const Hex centerHex{0, 0, 0};
    GridSet neighborGridIds;
    GridSystem::GetNeighborGridIds(centerHex, neighborGridIds);

    EXPECT_EQ(neighborGridIds.size(), 6);

    for (int direction = 0; direction < 6; ++direction)
    {
        auto expectedId = GridSystem::GetGridId(hex_neighbor(centerHex, direction));
        EXPECT_TRUE(neighborGridIds.contains(expectedId))
            << "Missing neighbor in direction " << direction;
    }
}

// Spawn 10 players at random positions; after each Update the entity must
// appear in the grid cell that corresponds to its world position.
TEST_F(AoiGridTest, RandomSpawnAssignsCorrectGrid)
{
    auto sceneEntity = tlsEcs.sceneRegistry.create();
    auto& sceneGridList = tlsEcs.sceneRegistry.get_or_emplace<SceneGridListComp>(sceneEntity);

    const SceneEntityComp sceneComp{sceneEntity};
    std::unordered_map<absl::uint128, uint32_t, absl::Hash<absl::uint128>> entityCountPerGrid;

    for (uint32_t i = 0; i < 10; ++i)
    {
        auto playerEntity = tlsEcs.actorRegistry.create();
        auto& transform = tlsEcs.actorRegistry.get_or_emplace<Transform>(playerEntity);
        transform.mutable_location()->set_x(tlsRandom.RandReal<double>(0, 1000));
        transform.mutable_location()->set_y(tlsRandom.RandReal<double>(0, 1000));
        tlsEcs.actorRegistry.emplace<SceneEntityComp>(playerEntity, sceneComp);

        AoiSystem::Update(0.1);

        auto gridId = GridSystem::GetGridId(transform.location());
        ++entityCountPerGrid[gridId];
        EXPECT_TRUE(sceneGridList[gridId].entities.contains(playerEntity));
        EXPECT_EQ(sceneGridList[gridId].entities.size(), entityCountPerGrid[gridId]);
    }

    for (auto&& [scene, gridList] : tlsEcs.sceneRegistry.view<SceneGridListComp>().each())
    {
        for (const auto& [_, grid] : gridList)
        {
            EXPECT_FALSE(grid.entities.empty());
        }
    }

    GridSystem::UpdateLogGridSize();
}

// Move a single player from the origin hex to each of its 6 neighbors and
// back, verifying grid membership after every move.
TEST_F(AoiGridTest, RoundTripThroughAllSixNeighbors)
{
    auto sceneEntity = tlsEcs.sceneRegistry.create();
    auto& sceneGridList = tlsEcs.sceneRegistry.emplace<SceneGridListComp>(sceneEntity);

    auto playerEntity = tlsEcs.actorRegistry.create();
    auto& transform = tlsEcs.actorRegistry.emplace<Transform>(playerEntity);
    transform.mutable_location()->set_x(0);
    transform.mutable_location()->set_y(0);
    tlsEcs.actorRegistry.emplace<SceneEntityComp>(playerEntity, SceneEntityComp{sceneEntity});

    AoiSystem::Update(0.1);

    const Hex originHex = hex_round(pixel_to_hex(kHexLayout, Point(0, 0)));
    const auto originGridId = GridSystem::GetGridId(originHex);
    EXPECT_TRUE(sceneGridList[originGridId].entities.contains(playerEntity));

    for (int direction = 0; direction < 6; ++direction)
    {
        // Move to neighbor
        const Hex neighborHex = hex_neighbor(originHex, direction);
        const Point neighborPixel = hex_to_pixel(kHexLayout, neighborHex);
        transform.mutable_location()->set_x(neighborPixel.x);
        transform.mutable_location()->set_y(neighborPixel.y);
        AoiSystem::Update(0.1);

        auto neighborGridId = GridSystem::GetGridId(neighborHex);
        EXPECT_TRUE(sceneGridList[neighborGridId].entities.contains(playerEntity))
            << "Player not in neighbor grid (direction " << direction << ")";
        EXPECT_FALSE(sceneGridList[originGridId].entities.contains(playerEntity))
            << "Player still in origin grid after moving (direction " << direction << ")";

        // Move back to origin
        transform.mutable_location()->set_x(0);
        transform.mutable_location()->set_y(0);
        AoiSystem::Update(0.1);
        EXPECT_TRUE(sceneGridList[originGridId].entities.contains(playerEntity));
        EXPECT_FALSE(sceneGridList[neighborGridId].entities.contains(playerEntity));
    }

    // After all round-trips only one grid cell should contain the player.
    std::size_t totalEntities = 0;
    for (auto&& [scene, gridList] : tlsEcs.sceneRegistry.view<SceneGridListComp>().each())
    {
        for (const auto& [_, grid] : gridList)
        {
            if (grid.entities.empty()) continue;
            EXPECT_EQ(grid.entities.size(), 1);
            totalEntities += grid.entities.size();
        }
    }
    EXPECT_EQ(totalEntities, 1);
}

// ---------------------------------------------------------------------------
// Fixture: Entity enter / leave visibility (verified via AoiListComp)
// ---------------------------------------------------------------------------

class AoiVisibilityTest : public ::testing::Test
{
protected:
    entt::entity entity1 = entt::null;
    entt::entity entity2 = entt::null;
    entt::entity sceneEntity = entt::null;

    void SetUp() override
    {
        RegisterGlobalMessageComponents();

        sceneEntity = tlsEcs.sceneRegistry.create();
        tlsEcs.sceneRegistry.emplace<SceneGridListComp>(sceneEntity);

        const SceneEntityComp sceneComp{sceneEntity};

        entity1 = tlsEcs.actorRegistry.create();
        tlsEcs.actorRegistry.emplace<Transform>(entity1);            // origin (0,0,0)
        tlsEcs.actorRegistry.emplace<SceneEntityComp>(entity1, sceneComp);

        entity2 = tlsEcs.actorRegistry.create();
        auto& t2 = tlsEcs.actorRegistry.emplace<Transform>(entity2);
        t2.mutable_location()->set_x(100);                           // far away initially
        t2.mutable_location()->set_y(100);
        tlsEcs.actorRegistry.emplace<SceneEntityComp>(entity2, sceneComp);
    }

    void TearDown() override
    {
        UnregisterGlobalMessageComponents();
        tlsEcs.actorRegistry.clear();
        tlsEcs.sceneRegistry.clear();
    }

    // Convenience: check whether |observer| has |target| in its AoiListComp.
    static bool IsInAoiList(entt::entity observer, entt::entity target)
    {
        const auto* aoiList = tlsEcs.actorRegistry.try_get<AoiListComp>(observer);
        return aoiList && aoiList->aoiList.contains(target);
    }
};

// When two entities are within kMaxViewRadius (10 units) after Update,
// each should appear in the other's AoiListComp.
TEST_F(AoiVisibilityTest, EntitiesWithinRangeEnterEachOthersAoiList)
{
    // Move entity2 close to entity1 (distance 5 < kMaxViewRadius 10)
    auto& location2 = *tlsEcs.actorRegistry.get<Transform>(entity2).mutable_location();
    location2.set_x(5);
    location2.set_y(0);

    AoiSystem::Update(0.0);

    EXPECT_TRUE(IsInAoiList(entity1, entity2))
        << "entity2 should be in entity1's AoiList after entering view range";
    EXPECT_TRUE(IsInAoiList(entity2, entity1))
        << "entity1 should be in entity2's AoiList after entering view range";
}

// Two entities at the same position see each other, then after one leaves
// the scene via BeforeLeaveSceneHandler it is removed from the other's AoiList.
TEST_F(AoiVisibilityTest, EntityRemovedFromAoiListAfterLeavingScene)
{
    // Place both at origin so they see each other.
    auto& location2 = *tlsEcs.actorRegistry.get<Transform>(entity2).mutable_location();
    location2.set_x(0);
    location2.set_y(0);

    AoiSystem::Update(0.0);

    // Precondition: mutual visibility established.
    ASSERT_TRUE(IsInAoiList(entity1, entity2));
    ASSERT_TRUE(IsInAoiList(entity2, entity1));

    // entity2 leaves the scene.
    BeforeLeaveScene leaveEvent;
    leaveEvent.set_entity(entt::to_integral(entity2));
    AoiSystem::BeforeLeaveSceneHandler(leaveEvent);

    // entity1 should no longer list entity2.
    EXPECT_FALSE(IsInAoiList(entity1, entity2))
        << "entity2 should be removed from entity1's AoiList after leaving scene";
}

// Entities far apart (distance > kMaxViewRadius) should NOT appear in
// each other's AoiList.
TEST_F(AoiVisibilityTest, EntitiesOutOfRangeAreNotVisible)
{
    // entity2 is already at (100,100), distance ≈ 141 >> kMaxViewRadius.
    AoiSystem::Update(0.0);

    EXPECT_FALSE(IsInAoiList(entity1, entity2))
        << "entity2 should NOT be in entity1's AoiList when out of range";
    EXPECT_FALSE(IsInAoiList(entity2, entity1))
        << "entity1 should NOT be in entity2's AoiList when out of range";
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
