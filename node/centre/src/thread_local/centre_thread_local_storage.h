#pragma once

#include "src/comp/player_list.h"
#include "src/network/gate_node.h"
#include "src/network/game_node.h"
#include "src/network/login_node.h"

using PlayerListMap = std::unordered_map<Guid, entt::entity>;

class CentreThreadLocalStorage
{
public:
	inline PlayerListMap& player_list() { return player_list_; }
	inline GameNodeList& game_node() { return game_node_list_; }
	inline GateNodes& gate_nodes() { return gate_nodes_; }
private:
	PlayerListMap player_list_;
	GameNodeList game_node_list_;
	GateNodes gate_nodes_;
};

extern thread_local CentreThreadLocalStorage centre_tls;

