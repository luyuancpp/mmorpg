#pragma once

#include "game_logic/scene/comp/grid_comp.h"

class AfterEnterScene;
class BeforeLeaveScene;

struct Hex;
struct SceneEntityComp;
class Transform;

class AoiSystem {
public:
    static void Update(double delta);
    static void BeforeLeaveSceneHandler(const BeforeLeaveScene& message);
private:
    static void LeaveGrid(const Hex& hex, SceneGridListComp& grid_list, entt::entity player);
    static void BroadCastLeaveGridMessage(const SceneGridListComp& grid_list, entt::entity player, const GridSet& leave_grid_set);
};
