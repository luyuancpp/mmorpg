#pragma once

#include "src/network/gate_player_list.h"
#include "src/network/login_node.h"

struct GateThreadStorage
{
	ClientSessions sessions_;
	LoginNodes login_nodes;

};

extern thread_local GateThreadStorage gate_tls;

