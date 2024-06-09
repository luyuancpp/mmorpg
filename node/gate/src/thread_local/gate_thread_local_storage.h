#pragma once

#include <memory>

#include "src/network/gate_player_list.h"
#include "src/network/game_node.h"
#include "src/network/login_node.h"

class GateThreadLocalStorage
{
public:
	GateThreadLocalStorage();
	inline LoginNodes& login_nodes() { return *login_nodes_; }
private:
	std::unique_ptr<LoginNodes> login_nodes_;
};

extern thread_local GateThreadLocalStorage gate_tls;

