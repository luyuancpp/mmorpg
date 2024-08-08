#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "game_logic/scene/comp/nav_comp.h"

struct EnterSceneParam;
struct CreateGameNodeSceneParam;
class OnSceneCreate;
class AfterEnterScene;
class BeforeLeaveScene;

class GameNodeSceneUtil
{
public:
    static void InitializeNodeScenes();
    static void RegisterSceneToCentre();
    static void RegisterSceneToCentre(entt::entity scene);
    static void HandleSceneCreation(const OnSceneCreate& message);
    static void HandleAfterEnterSceneEvent(const AfterEnterScene& message);
    static void HandleBeforeLeaveSceneEvent(const BeforeLeaveScene& message);
private:
};
