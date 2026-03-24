#pragma once

#include <absl/hash/hash.h>
#include <absl/numeric/int128.h>

#include "spatial/comp/grid.h"

class Transform;
struct Hex;
class Vector3;

class GridSystem
{
public:
    static absl::uint128 GetGridId(const Vector3& pos);
    static absl::uint128 GetGridId(const Hex& hex);
    static Hex CalculateHexPosition(const Transform& transform);
    static void GetNeighborGridIds(const Hex& hex, GridSet& gridSet);
    static void GetCurrentAndNeighborGridIds(const Hex& hex, GridSet& gridSet);
    static void GetEntitiesInGridAndNeighbors(entt::entity entity, EntityUnorderedSet& entities, bool excludingSelf);
    static void GetEntitiesInViewAndNearby(entt::entity entity, EntityUnorderedSet& entities);
    static void UpdateLogGridSize();
};