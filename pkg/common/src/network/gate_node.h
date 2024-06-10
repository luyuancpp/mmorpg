#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/util/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"

#include "common_proto/common.pb.h"

struct GateNode
{
	GateNode(const muduo::net::TcpConnectionPtr& conn)
		: session_(conn) {}

	RpcServerConnection session_;
};
using GateNodeClient = std::shared_ptr<GateNode>;
using GateNodes = std::unordered_map<uint32_t, GateNodeClient>;


