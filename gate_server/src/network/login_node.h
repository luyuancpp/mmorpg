#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/network/node_info.h"
#include "src/network/rpc_client.h"
#include "src/game_logic/game_registry.h"

#include "common.pb.h"

struct LoginNode
{
	RpcClientPtr login_session_;
	NodeInfo node_info_;
	EntityPtr entity_id;
};

using LoginNodes = std::unordered_map<uint32_t, LoginNode>;
extern thread_local LoginNodes g_login_nodes;
