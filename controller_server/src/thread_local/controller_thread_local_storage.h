#pragma once

#include "src/comp/player_list.h"

using PlayerListMap = std::unordered_map<Guid, EntityPtr>;

class ControllerThreadLocalStorage
{
public:
	PlayerListMap& player_list() { return player_list_; }
private:
	PlayerListMap player_list_;
};

extern thread_local ControllerThreadLocalStorage controller_tls;

