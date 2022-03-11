#ifndef GATEWAY_SERVER_GS_SESSION_H_
#define GATEWAY_SERVER_GS_SESSION_H_

#include "entt/src/entt/entity/registry.hpp"

#include "src/server_common/node_info.h"
#include "src/server_common/rpc_client.h"
#include "src/server_common/rpc_stub.h"

#include "gw2gs.pb.h"

namespace gateway
{
    struct GSSessionInfo
    {
		using Gw2gsStubPtr = std::unique_ptr<common::RpcStub<gw2gs::Gw2gsService_Stub>>;
        common::RpcClientPtr gs_session_;		
        Gw2gsStubPtr gw2gs_stub_;
        common::NodeInfo node_info_;
        entt::entity entity_id{ entt::null };
    };


    class GsSessionReg : public entt::registry
    {
    public:
        using GsSessions = std::unordered_map<uint32_t, GSSessionInfo>;
		GSSessionInfo* GetSeesion(uint32_t node_id)
		{
			auto it = gs_sessions_.find(node_id);
			if (it == gs_sessions_.end())
			{
				return nullptr;
			}
			return &it->second;
		}

        void AddGs(uint32_t node_id, GSSessionInfo&& gsi) { gs_sessions_.emplace(node_id, std::move(gsi)); }
        void RemoveGs(uint32_t node_id) { gs_sessions_.erase(node_id); }

    private:
        GsSessions gs_sessions_;
    };
    extern thread_local GsSessionReg g_gs_sesssion;
}//namespace gateway

#endif//GATEWAY_SERVER_GS_SESSION_H_
