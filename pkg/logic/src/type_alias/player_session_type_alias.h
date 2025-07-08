#pragma once
#include <unordered_map>

#include "type_define/type_define.h"
#include "thread_local/storage.h"

using SessionList = std::unordered_map<Guid, Guid>;

inline SessionList& GlobalSessionList()
{
	auto& list = tls.globalRegistry.get_or_emplace<SessionList>(GlobalEntity());
	return list;
}