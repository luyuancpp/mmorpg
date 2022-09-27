#pragma once

#include "entt/src/entt/entity/registry.hpp"

struct EnterSceneParam;
struct CreateSceneBySceneInfoP;

class GsSceneSystem
{
public:
	static void CreateSceneByGuid(CreateSceneBySceneInfoP& param);
    static void EnterScene(const EnterSceneParam& param);
    static void LeaveScene(entt::entity ent);
};
