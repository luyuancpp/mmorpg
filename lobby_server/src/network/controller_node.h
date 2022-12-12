#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/game_logic/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"
#include "src/network/rpc_client.h"

#include "common.pb.h"

struct ControllerNode
{

	ControllerNode(const muduo::net::TcpConnectionPtr& conn)
		: session_(conn) {}

	inline uint32_t node_id() const { return node_info_.node_id(); }
	NodeInfo node_info_;
	RpcServerConnection session_;
};
using ControllerNodePtr = std::shared_ptr<ControllerNode>;
using ControllerNodes = std::unordered_map<uint32_t, entt::entity>;
extern ControllerNodes* g_controller_nodes;


