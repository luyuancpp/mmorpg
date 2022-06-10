#ifndef GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_
#define GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_

#include "muduo/net/TcpConnection.h"

#include "src/game_logic/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_stub.h"

#include "component_proto/node_comp.pb.h"

using MasterSessionPtr = std::shared_ptr<RpcClient>;
struct MsNode
{
	MsNode(){}

	inline uint32_t node_id() const { return node_info_.node_id(); }
	NodeInfo node_info_;
	MasterSessionPtr session_;
	EntityPtr ms_;
};
using MsNodePtr = std::shared_ptr<MsNode>;
using MsNodes = std::unordered_map<uint32_t, MsNodePtr>;
extern MsNodes* g_ms_nodes;//master 不会删除，因为不会和gs一样根据负载增加减少，只走底层的自动重连


#endif//GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_
