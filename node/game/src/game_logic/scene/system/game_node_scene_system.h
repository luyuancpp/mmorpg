#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "game_logic/scene/comp/nav_comp.h"

struct EnterSceneParam;
struct CreateGameNodeSceneParam;
class OnSceneCreate;
class AfterEnterScene;
class BeforeLeaveScene;

class GameNodeSceneSystem
{
public:
    static void LoadAllMainSceneNavBin();
    static void InitNodeScene();
    static void EnterScene(const EnterSceneParam& param);
    static void LeaveScene(entt::entity ent);
    static void RegisterSceneToCentre();
    static void RegisterSceneToCentre(entt::entity scene);
    static void OnSceneCreateHandler(const OnSceneCreate& message);
    static void AfterEnterSceneHandler(const AfterEnterScene& message);
    static void BeforeLeaveSceneHandler(const BeforeLeaveScene& message);
private:
};
