#pragma once

#include <absl/hash/hash.h>
#include <absl/numeric/int128.h>

#include "game_logic/scene/comp/grid_comp.h"

class Transform;
struct Hex;
class Location;

class GridUtil
{
public:
    static absl::uint128 GetGridId(const Location& l);
    static absl::uint128 GetGridId(const Hex& hex);
    static Hex CalculateHexPosition(const Transform& transform);
    static void GetNeighborGridIds(const Hex& hex, GridSet& gridSet);
    static void GetCurrentAndNeighborGridIds(const Hex& hex, GridSet& gridSet);
    static void GetEntitiesInGridAndNeighbors(entt::entity entity, EntityUnorderedSet& entites, bool excludingSelf);
    static void GetEntitiesInViewAndNearby(entt::entity entity, EntityUnorderedSet& entites);
    static void UpdateLogGridSize(double delta);
    static void ClearEmptyGrids();
};