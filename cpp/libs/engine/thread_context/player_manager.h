#pragma once

#include <unordered_map>
#include "engine/core/type_define/type_define.h"

using PlayerListMap = std::unordered_map<Guid, entt::entity>;

extern thread_local PlayerListMap tlsPlayerList;


entt::entity GetPlayer(Guid player_uid);

