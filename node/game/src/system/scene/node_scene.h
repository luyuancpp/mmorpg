#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "comp/scene/nav.h"

struct EnterSceneParam;
struct CreateGameNodeSceneParam;
class OnSceneCreate;

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
};
