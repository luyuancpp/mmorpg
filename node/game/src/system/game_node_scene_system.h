#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "comp/gs_scene_comp.h"

struct EnterSceneParam;
struct CreateGameNodeSceneParam;

class GameNodeSceneSystem
{
public:
    static void LoadAllMainSceneNavBin();
    static void CreateNodeScene();
	static void CreateScene(CreateGameNodeSceneParam& param);
    static void EnterScene(const EnterSceneParam& param);
    static void LeaveScene(entt::entity ent);
};
