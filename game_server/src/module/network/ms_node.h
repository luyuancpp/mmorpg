#ifndef GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_
#define GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_

#include "muduo/net/TcpConnection.h"

#include "src/game_logic/game_registry.h"
#include "src/server_common/node_info.h"
#include "src/server_common/server_component.h"
#include "src/server_common/rpc_client.h"
#include "src/server_common/rpc_stub.h"

#include "common.pb.h"
#include "ms_node.pb.h"


using MasterSessionPtr = std::shared_ptr<common::RpcClient>;
struct MsNode
{
	using MsStubNode = common::RpcStub<msservice::MasterNodeService_Stub>;
	MsNode(){}

	inline uint32_t node_id() const { return node_info_.node_id(); }

	MsStubNode ms_stub_;
	NodeInfo node_info_;
	MasterSessionPtr session_;
};
using MsNodePtr = std::shared_ptr<MsNode>;
using MsNodeWPtr = std::weak_ptr<MsNode>;
using MsNodes = std::unordered_map<uint32_t, MsNodePtr>;
extern MsNodes g_ms_nodes;


#endif//GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_
