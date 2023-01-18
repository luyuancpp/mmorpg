#pragma once

#include "src/network/gate_player_list.h"
#include "src/network/login_node.h"

struct GateThreadLocalStorage
{
	ClientSessions sessions_;
	LoginNodes login_nodes;

};

extern thread_local GateThreadLocalStorage gate_tls;

