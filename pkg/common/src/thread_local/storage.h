#pragma once

#include "util/game_registry.h"

#include <type_define/type_define.h>


class ThreadLocalStorage
{
public:
	ThreadLocalStorage();


	entt::entity operatorEntity{ entt::null };
	entt::entity errorEntity{ entt::null };
	entt::entity globalEntity{ entt::null };
	entt::entity globalGrpcNodeEntity{ entt::null };

	void Clear();

private:
};

extern thread_local ThreadLocalStorage tls;