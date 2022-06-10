#ifndef GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_
#define GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_

#include "muduo/net/TcpConnection.h"

#include "src/game_logic/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_stub.h"

#include "component_proto/node_comp.pb.h"
#include "ms_service.pb.h"


struct MsNode
{
	using MsStubNode = RpcStub<msservice::MasterNodeService_Stub>;
	MsNode(const muduo::net::TcpConnectionPtr& conn)
		: session_(conn) {}

	inline uint32_t node_id() const { return node_info_.node_id(); }

	MsStubNode ms_stub_;
	NodeInfo node_info_;
	RpcServerConnection session_;
};
using MsNodePtr = std::shared_ptr<MsNode>;
using MsNodeWPtr = std::weak_ptr<MsNode>;
using MsNodes = std::unordered_map<uint32_t, entt::entity>;
extern MsNodes* g_ms_nodes;


#endif//GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_
