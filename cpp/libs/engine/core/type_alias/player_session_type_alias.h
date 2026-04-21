#pragma once
#include <unordered_map>

#include "type_define/type_define.h"

#include <thread_context/ecs_context.h>

using SessionList = std::unordered_map<SessionId, Guid>;

inline SessionList& SessionMap()
{
	auto& list = tlsEcs.globalRegistry.get_or_emplace<SessionList>(tlsEcs.GlobalEntity());
	return list;
}
