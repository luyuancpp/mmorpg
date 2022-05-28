#ifndef GATEWAY_SERVER_GS_SESSION_H_
#define GATEWAY_SERVER_GS_SESSION_H_

#include "entt/src/entt/entity/registry.hpp"

#include "src/network/node_info.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_stub.h"
#include "src/game_logic/game_registry.h"

#include "gs_service.pb.h"
#include "component_proto/node_comp.pb.h"

struct GsNode
{
	using GsStubPtr = std::unique_ptr<RpcStub<gsservice::GsService_Stub>>;
	RpcClientPtr gs_session_;
	GsStubPtr gs_stub_;
	NodeInfo node_info_;
	EntityPtr entity_id;
};

using GsNodes = std::unordered_map<uint32_t, GsNode>;
extern thread_local GsNodes g_gs_nodes;


#endif//GATEWAY_SERVER_GS_SESSION_H_
