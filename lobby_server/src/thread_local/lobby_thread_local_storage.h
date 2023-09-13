#pragma once

#include "src/network/gs_node.h"
#include "src/network/controller_node.h"

class LobbyThreadLocalStorage
{
public:
	ControllerNodes& controller_nodes() { return controller_nodes_; }
	GameNodeList& game_node_list() { return game_node_list_; }
private:
	ControllerNodes controller_nodes_;
	GameNodeList game_node_list_;
};

extern thread_local LobbyThreadLocalStorage lobby_tls;

