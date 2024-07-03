#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "comp/game_node_scene.h"

struct EnterSceneParam;
struct CreateGameNodeSceneParam;

class GameNodeSceneSystem
{
public:
    static void LoadAllMainSceneNavBin();
    static void InitNodeScene();
	static void CreateScene(CreateGameNodeSceneParam& p);
    static void EnterScene(const EnterSceneParam& param);
    static void LeaveScene(entt::entity ent);
    static void RegisterSceneToCentre();
    static void RegisterSceneToCentre(entt::entity scene);
};
