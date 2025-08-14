#pragma once

#include <unordered_map>
#include "type_define/type_define.h"

using PlayerListMap = std::unordered_map<Guid, entt::entity>;

extern thread_local PlayerListMap gPlayerList;

class PlayerManager {
public:
	static PlayerManager& Instance() {
		thread_local PlayerManager instance;
		return instance;
	}

	entt::entity GetPlayer(Guid player_uid);
};


