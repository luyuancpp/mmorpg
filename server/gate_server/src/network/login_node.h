#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/network/node_info.h"
#include "src/network/rpc_client.h"
#include "src/util/game_registry.h"
#include "src/util/consistent_hash_node.h"

#include "common_proto/common.pb.h"

struct LoginNode
{
	RpcClientPtr login_session_;
	NodeInfo node_info_;
};

using LoginNodes = ConsistentHashNode<SessionId, LoginNode>;

