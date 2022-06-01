#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

class EnterGsInfo;

class PlayerNetworkSystem
{
public:
	static void EnterGs(entt::entity player, uint64_t session_id, const EnterGsInfo& enter_info);
};
