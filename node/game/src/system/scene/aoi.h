#pragma once

#include <absl/hash/hash.h>
#include <absl/numeric/int128.h>

#include "comp/scene/grid.h"

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
    static void ScanNeighborGridId(const Hex& hex, GridSet& grid_set);
    static void HandlePlayerMovement(entt::entity mover, const Transform& transform, SceneEntityComp& player_scene);
    static void LeaveGrid(const Hex& hex, SceneGridList& grid_list, entt::entity player);
private:
    static void BroadCastLeaveGridMessage(const SceneGridList& grid_list, entt::entity player, const GridSet& leave_grid_set);
};
