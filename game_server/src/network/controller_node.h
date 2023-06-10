#pragma once
#include "muduo/net/TcpConnection.h"

#include "src/util/game_registry.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"
#include "src/network/rpc_client.h"

#include "common.pb.h"

using ControllerSessionPtr = std::shared_ptr<RpcClient>;
struct ControllerNode
{
	ControllerNode(){}

	inline uint32_t node_id() const { return node_info_.node_id(); }
	NodeInfo node_info_;
	ControllerSessionPtr session_;
	entt::entity controller_{entt::null};
};
using ControllerNodePtr = std::shared_ptr<ControllerNode>;
using ControllerNodes = std::unordered_map<uint64_t, ControllerNodePtr>;


