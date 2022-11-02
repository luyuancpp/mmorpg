#ifndef GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_
#define GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_

#include "muduo/net/TcpConnection.h"

#include "src/game_logic/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_stub.h"

#include "component_proto/node_comp.pb.h"
#include "controller_service.pb.h"

struct ControllerNode
{
	using ControllerStub = RpcStub<controllerservice::ControllerNodeService_Stub>;
	ControllerNode(const muduo::net::TcpConnectionPtr& conn)
		: session_(conn) {}

	inline uint32_t node_id() const { return node_info_.node_id(); }

	ControllerStub controller_stub_;
	NodeInfo node_info_;
	RpcServerConnection session_;
};
using ControllerNodePtr = std::shared_ptr<ControllerNode>;
using ControllerNodeWPtr = std::weak_ptr<ControllerNode>;
using ControllerNodes = std::unordered_map<uint32_t, entt::entity>;
extern ControllerNodes* g_controller_nodes;


#endif//GAME_SERVER_MODULE_NETWORK_MS_NODE_NODE_H_
