#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/network/node_info.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_stub.h"
#include "src/game_logic/game_registry.h"

#include "login_service.pb.h"
#include "component_proto/node_comp.pb.h"

struct LoginNode
{
	using LoginStubPtr = std::unique_ptr<RpcStub<gw2l::LoginService_Stub>>;
	RpcClientPtr login_session_;
	LoginStubPtr login_stub_;
	NodeInfo node_info_;
	EntityPtr entity_id;
};

using LoginNodes = std::unordered_map<uint32_t, LoginNode>;
extern thread_local LoginNodes g_login_nodes;
