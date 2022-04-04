#ifndef MASTER_SERVER_GAME_GAME_CLIENT_H_
#define MASTER_SERVER_GAME_GAME_CLIENT_H_

#include "muduo/net/TcpConnection.h"

#include "src/game_logic/entity/entity.h"
#include "src/server_common/node_info.h"
#include "src/server_common/rpc_stub.h"
#include "src/server_common/server_component.h"

#include "common.pb.h"

struct GsNode
{
	GsNode(const muduo::net::TcpConnectionPtr& conn)
		: session_(conn){}
	NodeInfo node_info_;
	common::RpcServerConnection session_;
};

using GsNodePtr = std::shared_ptr<GsNode>;

struct GsNodes : public std::unordered_map<uint32_t, entt::entity> {};


#endif//MASTER_SERVER_GAME_GAME_CLIENT_H_
