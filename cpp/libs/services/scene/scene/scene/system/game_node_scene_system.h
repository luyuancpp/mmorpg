#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "scene/scene/comp/nav_comp.h"

struct EnterRoomParam;
struct CreateSceneNodeRoomParam;
class OnSceneCreate;
class AfterEnterScene;
class BeforeLeaveScene;

class GameNodeSceneSystem
{
public:
    static void InitializeNodeScenes();
    static void RegisterSceneToAllCentre(entt::entity scene);
	static void RegisterAllSceneToCentre(entt::entity centre);
    static void HandleSceneCreation(const OnSceneCreate& message);
    static void HandleAfterEnterSceneEvent(const AfterEnterScene& message);
    static void HandleBeforeLeaveSceneEvent(const BeforeLeaveScene& message);
private:
};
