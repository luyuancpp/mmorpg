#pragma once
#include <unordered_map>

#include "type_define/type_define.h"

#include "component_proto/player_network_comp.pb.h"

using SessionList = std::unordered_map<Guid, PlayerSessionInfo>;

#define tls_sessions tls.global_registry.emplace<SessionList>(global_entity())