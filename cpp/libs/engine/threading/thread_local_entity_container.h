#pragma once

#include "core/utils/registry/game_registry.h"

#include <engine/core/type_define/type_define.h>


class ThreadLocalEntityContainer
{
public:
	ThreadLocalEntityContainer();

	entt::entity operatorEntity{ entt::null };
	entt::entity errorEntity{ entt::null };
	entt::entity globalEntity{ entt::null };
	entt::entity globalGrpcNodeEntity{ entt::null };

	void Clear();

private:
};

extern thread_local ThreadLocalEntityContainer tlsThreadLocalEntityContainer;