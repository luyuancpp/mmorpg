#pragma once

#include "src/comp/player_list.h"
#include "src/network/gs_node.h"

using PlayerListMap = std::unordered_map<Guid, EntityPtr>;
using GateSessionList = std::unordered_map<uint64_t, EntityPtr>;
using GateNodes = std::unordered_map<uint32_t, entt::entity>;

class ControllerThreadLocalStorage
{
public:
	PlayerListMap& player_list() { return player_list_; }
	GsNodes& game_node() { return game_node_; }
	GateSessionList& gate_sessions() {return gate_sessions_;}
	GateNodes& gate_nodes() { return gate_nodes_; }
private:
	PlayerListMap player_list_;
	GsNodes game_node_;
	GateSessionList gate_sessions_;
	GateNodes gate_nodes_;
};

extern thread_local ControllerThreadLocalStorage controller_tls;

