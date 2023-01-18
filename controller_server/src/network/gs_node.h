#pragma once
#include "muduo/net/TcpConnection.h"

#include "src/game_logic/thread_local/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"

#include "common.pb.h"
#include "component_proto/gs_node_comp.pb.h"

struct GsNode
{
	GsNode(const muduo::net::TcpConnectionPtr& conn)
		: session_(conn){}
	decltype(auto) node_id() const { return node_info_.node_id(); }
	NodeInfo node_info_;
	RpcServerConnection session_;
};

struct GsServer {};

using GsNodePtr = std::shared_ptr<GsNode>;
using GsNodePlayerInfoPtr = std::shared_ptr<GsNodePlayerInfo>;

using GsNodes = std::unordered_map<uint32_t, entt::entity>;
extern GsNodes g_game_node;

