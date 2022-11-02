#pragma once
#include "muduo/net/TcpConnection.h"

#include "src/game_logic/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_stub.h"

#include "component_proto/node_comp.pb.h"

using ControllerSessionPtr = std::shared_ptr<RpcClient>;
struct ControllerNode
{
	ControllerNode(){}

	inline uint32_t node_id() const { return node_info_.node_id(); }
	NodeInfo node_info_;
	ControllerSessionPtr session_;
	EntityPtr controller_;
};
using ControllerNodePtr = std::shared_ptr<ControllerNode>;
using ControllerNodes = std::unordered_map<uint32_t, ControllerNodePtr>;
extern ControllerNodes* g_controller_nodes;//controller ����ɾ������Ϊ�����gsһ�����ݸ������Ӽ��٣�ֻ�ߵײ���Զ�����


