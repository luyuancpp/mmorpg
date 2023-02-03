#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/util/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"

#include "common.pb.h"

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
