#ifndef MASTER_SERVER_GAME_GAME_CLIENT_H_
#define MASTER_SERVER_GAME_GAME_CLIENT_H_

#include "muduo/net/TcpConnection.h"

#include "src/game_logic/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/rpc_stub.h"
#include "src/network/server_component.h"

#include "component_proto/node_comp.pb.h"

struct GsNode
{
	GsNode(const muduo::net::TcpConnectionPtr& conn)
		: session_(conn){}
	NodeInfo node_info_;
	RpcServerConnection session_;
};

using GsNodePtr = std::shared_ptr<GsNode>;

using GsNodes = std::unordered_map<uint32_t, entt::entity>;
extern GsNodes* g_gs_nodes;

#endif//MASTER_SERVER_GAME_GAME_CLIENT_H_
