#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

class EnterGsInfo;

class PlayerSceneSystem
{
public:
	static void EnterScene(entt::entity player, const EnterGsInfo& enter_info, uint64_t session_id);
};
