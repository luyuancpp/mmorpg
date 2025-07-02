#pragma once
#include <unordered_map>

#include "type_define/type_define.h"

using SessionList = std::unordered_map<Guid, Guid>;

#define tlsSessions tls.globalRegistry.get<SessionList>(GlobalEntity())