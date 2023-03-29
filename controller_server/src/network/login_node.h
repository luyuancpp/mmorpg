#pragma once
#include "muduo/net/TcpConnection.h"

#include "src/network/node_info.h"
#include "src/network/server_component.h"

#include "common.pb.h"

class LoginNode
{
public:
	LoginNode(const muduo::net::TcpConnectionPtr& conn);
	~LoginNode();
	decltype(auto) node_id() const { return node_info_.node_id(); }
	NodeInfo node_info_;
	RpcServerConnection session_;
};

using LoginNodes = std::unordered_map<uint32_t, entt::entity>;


