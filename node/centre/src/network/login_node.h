#pragma once
#include "muduo/net/TcpConnection.h"

#include "src/network/node_info.h"
#include "src/network/rpc_session.h"

#include "common_proto/common.pb.h"

class LoginNode
{
public:
	LoginNode(const muduo::net::TcpConnectionPtr& conn) : session_(conn) {}
	~LoginNode() {}
	decltype(auto) node_id() const { return node_info_.node_id(); }
	NodeInfo node_info_;
	RpcSession session_;
};

using LoginNodes = std::unordered_map<uint32_t, entt::entity>;


