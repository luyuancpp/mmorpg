#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

class PlayerSceneSystem
{
public:
	static void EnterScene(entt::entity player, uint64_t scene_id);
	static void OnEnterScene(entt::entity player, entt::entity scene);
	static void LeaveScene(entt::entity player);
};
