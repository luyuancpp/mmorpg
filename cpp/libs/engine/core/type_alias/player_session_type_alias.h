#pragma once
#include <unordered_map>

#include "type_define/type_define.h"

#include <threading/registry_manager.h>
#include <threading/entity_manager.h>

using SessionList = std::unordered_map<Guid, Guid>;

inline SessionList& SessionMap()
{
	auto& list = tlsRegistryManager.globalRegistry.get_or_emplace<SessionList>(GlobalEntity());
	return list;
}