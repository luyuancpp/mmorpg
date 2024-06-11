#pragma once
#include "muduo/net/TcpConnection.h"

#include "src/util/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/rpc_session.h"

#include "common_proto/common.pb.h"
#include "component_proto/gs_node_comp.pb.h"

class GameNode
{
public:
	explicit GameNode(const muduo::net::TcpConnectionPtr& conn) : session_(conn)
	{
	}

	RpcSession session_;
};

using GameNodeClient = std::shared_ptr<GameNode>;
using GameNodePlayerInfoPtr = std::shared_ptr<GameNodeInfo>;



