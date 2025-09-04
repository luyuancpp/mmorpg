#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "scene/scene/comp/nav_comp.h"

struct EnterRoomParam;
struct CreateRoomOnNodeRoomParam;
class OnRoomCreated;
class AfterEnterRoom;
class BeforeLeaveRoom;

class GameNodeSceneSystem
{
public:
    static void InitializeNodeScenes();
    static void RegisterSceneToAllCentre(entt::entity scene);
	static void RegisterAllSceneToCentre(entt::entity centre);
    static void HandleSceneCreation(const OnRoomCreated& message);
    static void HandleAfterEnterSceneEvent(const AfterEnterRoom& message);
    static void HandleBeforeLeaveSceneEvent(const BeforeLeaveRoom& message);
private:
};
