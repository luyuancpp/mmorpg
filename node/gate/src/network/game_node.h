#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/network/node_info.h"
#include "src/network/rpc_client.h"
#include "src/util/game_registry.h"

#include "common_proto/common.pb.h"

struct GameNode
{
	RpcClientPtr gs_session_;
	NodeInfo node_info_;
};

using GameNodeList = std::unordered_map<uint32_t, GameNode>;

