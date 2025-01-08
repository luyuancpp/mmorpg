﻿#pragma once

#include "scene/comp/grid_comp.h"

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
    static void UpdateGridState(entt::entity entity, SceneGridListComp& gridList, const Hex& currentHex,
                                GridId currentGridId, GridSet& gridsToEnter, GridSet& gridsToLeave);
    static void HandleEntityVisibility(entt::entity entity, SceneGridListComp& gridList,
                                       const GridSet& gridsToEnter, const GridSet& gridsToLeave);
    static void NotifyEntityVisibilityChanges(entt::entity entity,
                                              const EntityUnorderedSet& enteringEntities, 
                                              const EntityUnorderedSet& leavingEntities);
    static void RemoveEntityFromGrid(const Hex& hex, SceneGridListComp& gridList, entt::entity entity) ;

    static void BroadcastEntityLeave(const SceneGridListComp& gridList, entt::entity entity, const GridSet& gridsToLeave);
};
