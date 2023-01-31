#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/comp/gs_scene_comp.h"

struct EnterSceneParam;
struct CreateSceneBySceneInfoP;

class GsSceneSystem
{
public:
    static void LoadAllMainSceneNavBin();
	static void CreateSceneByGuid(CreateSceneBySceneInfoP& param);
    static void EnterScene(const EnterSceneParam& param);
    static void LeaveScene(entt::entity ent);
};
