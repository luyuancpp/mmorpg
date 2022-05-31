#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

class EnterSceneInfo;

class PlayerSceneSystem
{
public:
	static void EnterScene(entt::entity player, const EnterSceneInfo& enter_info, uint64_t session_id);
};
