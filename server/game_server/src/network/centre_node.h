#pragma once
#include "muduo/net/TcpConnection.h"

#include "src/util/game_registry.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/node_info.h"
#include "src/network/server_component.h"
#include "src/network/rpc_client.h"

#include "common_proto/common.pb.h"

using CentreSessionPtr = std::shared_ptr<RpcClient>;

struct ControllerNode
{
	inline uint32_t node_id() const { return node_info_.node_id(); }
	NodeInfo node_info_;
	CentreSessionPtr session_;
};
using ControllerNodePtr = std::shared_ptr<ControllerNode>;
using CentreNodes = std::unordered_map<uint64_t, ControllerNodePtr>;


