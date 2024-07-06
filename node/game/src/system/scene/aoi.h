#pragma once

#include <absl/hash/hash.h>
#include <absl/numeric/int128.h>

#include "comp/scene/grid.h"

class AfterEnterScene;
class BeforeLeaveScene;
class Location;
struct Hex;

class AoiSystem
{
public:
    static void Update(double delta);
    static inline absl::uint128 GetGridId(const Location& l);
    static inline absl::uint128 GetGridId(const Hex& hex);
    static inline void ScanNeighborGridId(const Hex& hex, GridSet& grid_set);
    static void BeforeLeaveSceneHandler(const BeforeLeaveScene& message);
private:
    static inline void LeaveGrid(const Hex& hex, SceneGridList& grid_list, entt::entity player);
};
