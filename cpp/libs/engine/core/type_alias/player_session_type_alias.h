#pragma once
#include <unordered_map>

#include "type_define/type_define.h"

#include <thread_context/registry_manager.h>
#include <thread_context/entity_manager.h>

using SessionList = std::unordered_map<Guid, Guid>;

inline SessionList& SessionMap()
{
	auto& list = tlsEcs.globalRegistry.get_or_emplace<SessionList>(tlsEcs.GlobalEntity());
	return list;
}
