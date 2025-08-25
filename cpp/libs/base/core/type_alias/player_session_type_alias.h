#pragma once
#include <unordered_map>

#include "base/common/type_define/type_define.h"

#include <base/threading/registry_manager.h>
#include <threading/entity_manager.h>

using SessionList = std::unordered_map<Guid, Guid>;

inline SessionList& GlobalSessionList()
{
	auto& list = tlsRegistryManager.globalRegistry.get_or_emplace<SessionList>(GlobalEntity());
	return list;
}