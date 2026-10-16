#pragma once
#include <thread_context/ecs_context.h>

// 路由索引归 EcsContext::sessionPlayers 所有,随上下文清理,不再借全局实体暂存。
inline SessionPlayerIndex& SessionMap()
{
	return tlsEcs.sessionPlayers;
}
