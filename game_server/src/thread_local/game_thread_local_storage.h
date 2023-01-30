#pragma once

#include "src/game_logic/player/player_list.h"
#include "src/network/node_info.h"
#include "src/network/gate_node.h"
#include "src/network/controller_node.h"
#include "src/system/redis_system.h"

class GameThreadLocalStorage
{
public:
	RedisSystem& redis_system() { return redis_system_; }
	ControllerNodes& controller_node() { return controller_node_; }
	GateNodes& gate_node() { return gate_nodes_; }
	GateSessionList& gate_sessions() { return gate_sessions_; }
	PlayerList& player_list() { return player_list_; }
private:
	RedisSystem redis_system_;
	ControllerNodes controller_node_;
	GateNodes gate_nodes_;
	GateSessionList gate_sessions_;
	PlayerList player_list_;
};

extern thread_local GameThreadLocalStorage game_tls;

