#ifndef MASTER_SERVER_NETWORK_GATE_NODE_H_
#define MASTER_SERVER_NETWORK_GATE_NODE_H_

#include "muduo/net/TcpConnection.h"

#include "src/game_logic/game_registry.h"
#include "src/server_common/node_info.h"
#include "src/server_common/server_component.h"

#include "common.pb.h"


struct GateNode
{
	GateNode(const muduo::net::TcpConnectionPtr& conn)
		: session_(conn) {}

	inline uint32_t node_id() const { return node_info_.node_id(); }

	NodeInfo node_info_;
	common::RpcServerConnection session_;
};
using GateNodePtr = std::shared_ptr<GateNode>;
using GateNodeWPtr = std::weak_ptr<GateNode>;
using GateNodes = std::unordered_map<uint32_t, entt::entity>;
extern GateNodes g_gate_nodes;

#endif//MASTER_SERVER_NETWORK_GATE_NODE_H_
