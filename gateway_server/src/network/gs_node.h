#ifndef GATEWAY_SERVER_GS_SESSION_H_
#define GATEWAY_SERVER_GS_SESSION_H_

#include "entt/src/entt/entity/registry.hpp"

#include "src/server_common/node_info.h"
#include "src/server_common/rpc_client.h"
#include "src/server_common/rpc_stub.h"
#include "src/game_logic/entity/entity.h"

#include "gw2gs.pb.h"
#include "common.pb.h"

namespace gateway
{
    struct GsNode
    {
		using Gw2gsStubPtr = std::unique_ptr<common::RpcStub<gw2gs::Gw2gsService_Stub>>;
        common::RpcClientPtr gs_session_;		
        Gw2gsStubPtr gw2gs_stub_;
        NodeInfo node_info_;
        common::EntityPtr entity_id;
    };

    using GsNodes = std::unordered_map<uint32_t, GsNode>;
    extern thread_local GsNodes g_gs_nodes;
}//namespace gateway

#endif//GATEWAY_SERVER_GS_SESSION_H_
