#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/comp/gs_scene_comp.h"

struct EnterSceneParam;
struct CreateGameNodeSceneParam;

class GsSceneSystem
{
public:
    static void LoadAllMainSceneNavBin();
	static void CreateScene(CreateGameNodeSceneParam& param);
    static void EnterScene(const EnterSceneParam& param);
    static void LeaveScene(entt::entity ent);
};
