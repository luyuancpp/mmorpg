#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/network/node_info.h"
#include "src/network/rpc_client.h"
#include "src/game_logic/game_registry.h"

#include "common.pb.h"

struct GsNode
{
	RpcClientPtr gs_session_;
	NodeInfo node_info_;
	EntityPtr entity_id;
};

using GsNodes = std::unordered_map<uint32_t, GsNode>;
extern thread_local GsNodes g_gs_nodes;

