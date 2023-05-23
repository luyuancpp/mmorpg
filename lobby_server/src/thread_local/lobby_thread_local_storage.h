#pragma once

#include "src/network/gs_node.h"
#include "src/network/controller_node.h"

class LobbyThreadLocalStorage
{
public:
	ControllerNodes& controller_nodes() { return controller_nodes_; }
	GsNodes& gs_node() { return gs_node_; }
private:
	ControllerNodes controller_nodes_;
	GsNodes gs_node_;
};

extern thread_local LobbyThreadLocalStorage lobby_tls;

