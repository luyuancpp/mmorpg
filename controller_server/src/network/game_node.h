#pragma once
#include "muduo/net/TcpConnection.h"

#include "src/util/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"

#include "common.pb.h"
#include "component_proto/gs_node_comp.pb.h"

class GameNode
{
public:
	explicit GameNode(const muduo::net::TcpConnectionPtr& conn) : session_(conn)
	{
	}

	decltype(auto) node_id() const { return node_info_.node_id(); }
	NodeInfo node_info_;
	RpcServerConnection session_;
	muduo::net::InetAddress node_inet_addr_;
	entt::entity server_entity_{entt::null};
};

using GameNodePtr = std::shared_ptr<GameNode>;
using GsNodeWPtr = std::weak_ptr<GameNode>; //todo
using GameNodePlayerInfoPtr = std::shared_ptr<GameNodeInfo>;

using GameNodeList = std::unordered_map<uint32_t, entt::entity>;


