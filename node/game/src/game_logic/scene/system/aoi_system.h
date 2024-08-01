#pragma once

#include <absl/hash/hash.h>
#include <absl/numeric/int128.h>

#include "game_logic/scene/comp/grid.h"

class AfterEnterScene;
class BeforeLeaveScene;
class Location;
struct Hex;
struct SceneEntityComp;
class Transform;

class AoiSystem {
public:
    static void Update(double delta);
    static void BeforeLeaveSceneHandler(const BeforeLeaveScene& message);
    static void UpdateLogGridSize(double delta);
    static absl::uint128 GetGridId(const Location& l);
    static absl::uint128 GetGridId(const Hex& hex);
    static void ScanNeighborGridIds(const Hex& hex, GridSet& grid_set);
    static void ScanCurrentAndNeighborGridIds(const Hex& hex, GridSet& grid_set);
    static void LeaveGrid(const Hex& hex, SceneGridList& grid_list, entt::entity player);
    static void ClearEmptyGrids();
private:
    static void BroadCastLeaveGridMessage(const SceneGridList& grid_list, entt::entity player, const GridSet& leave_grid_set);
    static Hex CalculateHexPosition(const Transform& transform);
};
