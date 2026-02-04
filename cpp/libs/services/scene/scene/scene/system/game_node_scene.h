#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "scene/scene/comp/nav.h"

struct EnterRoomParam;
struct CreateRoomOnNodeRoomParam;
class OnRoomCreated;
class AfterEnterRoom;
class BeforeLeaveRoom;

class GameNodeSceneSystem
{
public:
    static void InitializeNodeScenes();
	static void RegisterAllSceneToCentre(entt::entity centre);
    static void HandleSceneCreation(const OnRoomCreated& message);
    static void HandleAfterEnterSceneEvent(const AfterEnterRoom& message);
    static void HandleBeforeLeaveSceneEvent(const BeforeLeaveRoom& message);
private:
};
