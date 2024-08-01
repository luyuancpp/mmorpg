#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "type_define/type_define.h"

class PlayerSceneSystem
{
public:
	static void EnterScene(entt::entity player, Guid scene);
	static void OnEnterScene(entt::entity player, entt::entity scene);
	static void LeaveScene(entt::entity player);
};
