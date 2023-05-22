#pragma once

#include <memory>

#include "src/network/gate_player_list.h"
#include "src/network/gs_node.h"
#include "src/network/login_node.h"

class GateThreadLocalStorage
{
public:
	GateThreadLocalStorage();
	inline ClientSessions& sessions() { return *sessions_; }
	inline LoginNodes& login_nodes() { return *login_nodes_; }
	inline GsNodes& game_nodes() { return *game_nodes_; }
private:
	std::unique_ptr<ClientSessions> sessions_;
	std::unique_ptr<LoginNodes> login_nodes_;
	std::unique_ptr<GsNodes> game_nodes_;
};

extern thread_local GateThreadLocalStorage gate_tls;

