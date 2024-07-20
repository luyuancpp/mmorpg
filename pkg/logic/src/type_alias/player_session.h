#pragma once
#include <unordered_map>

#include "type_define/type_define.h"

#include "proto/logic/component/player_network_comp.pb.h"

using SessionList = std::unordered_map<Guid, PlayerSessionInfo>;

#define tls_sessions tls.global_registry.get<SessionList>(global_entity())