#ifndef GAME_SERVER_MODULE_NETWORK_GATE_NODE_H_
#define GAME_SERVER_MODULE_NETWORK_GATE_NODE_H_

#include "muduo/net/TcpConnection.h"

#include "src/game_logic/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"

#include "component_proto/node_comp.pb.h"

struct GateNode
{
	GateNode(const muduo::net::TcpConnectionPtr& conn)
		: session_(conn) {}

	inline uint32_t node_id() const { return node_info_.node_id(); }

	NodeInfo node_info_;
	RpcServerConnection session_;
};
using GateNodePtr = std::shared_ptr<GateNode>;
using GateNodeWPtr = std::weak_ptr<GateNode>;
using GateNodes = std::unordered_map<uint32_t, entt::entity>;
extern GateNodes* g_gate_nodes;

using GateSessionList = std::unordered_map<uint64_t, entt::entity>;
using SessionPlayerList = std::unordered_map <uint64_t, uint64_t>;
extern SessionPlayerList* g_player_session_map;//ª·ª∞µΩplayer”≥…‰
extern GateSessionList* g_gate_sessions;

#endif//GAME_SERVER_MODULE_NETWORK_GATE_NODE_H_
