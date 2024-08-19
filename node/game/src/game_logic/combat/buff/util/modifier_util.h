#pragma once
#include <entt/src/entt/entity/entity.hpp>

class ModifierBuffUtil {
public:

	static void ModifyState(entt::entity parent, uint64_t buffId);
	
	static void ModifyAttributes(entt::entity parent, uint64_t buffId);
};


