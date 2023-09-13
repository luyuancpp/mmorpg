#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/util/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"

#include "common.pb.h"

struct GameNode
{
	GameNode(const muduo::net::TcpConnectionPtr& conn)
		: session_(conn){}
	
	decltype(auto) node_id() const { return node_info_.node_id(); }

	NodeInfo node_info_;
	RpcServerConnection session_;
};

using GameNodePtr = std::shared_ptr<GameNode>;

using GameNodeList = std::unordered_map<uint32_t, entt::entity>;

