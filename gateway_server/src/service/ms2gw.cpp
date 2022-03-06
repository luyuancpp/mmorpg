#include "ms2gw.h"

///<<< BEGIN WRITING YOUR CODE
#include "src/gs/gs_session.h"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/entity_cast.h"
#include "src/gate_player/gate_player_list.h"
#include "src/gateway_server.h"
#include "src/server_common/closure_auto_done.h"

#include "c2gs.pb.h"
#include "gw2gs.pb.h"

using namespace common;
using namespace gateway;
///<<< END WRITING YOUR CODE

namespace ms2gw{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

    /// ///<<<rpc begin
void Ms2gwServiceImpl::StartGS(::google::protobuf::RpcController* controller,
    const ms2gw::StartGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartGS
    InetAddress gs_addr(request->ip(), request->port());
    for (auto e : reg.view<InetAddress>())
    {
        auto& c = reg.get<InetAddress>(e);
        if (gs_addr.toIpPort() == c.toIpPort())// to do node id
        {
            return;
        }
    }
    GSSessionInfo gsi;
    gsi.node_info_.node_id_ = request->node_id();
    gsi.node_info_.node_type_ = GAME_SERVER_NODTE_TYPE;
    gsi.gs_session_ = std::make_unique<RpcClient>(EventLoop::getEventLoopOfCurrentThread(), gs_addr);
    gsi.gw2gs_stub_ = std::make_unique<RpcStub<gw2gs::Gw2gsService_Stub>>();
    gsi.c2gs_stub_proxy_ =std::make_unique<RpcStub<c2gs::C2GsService_Stub>>();
    //gsi.entity_id = GsSessionReg::GetSingleton().create();
    gsi.gs_session_->subscribe<RegisterStubEvent>(*(gsi.gw2gs_stub_.get()));
    gsi.gs_session_->subscribe<RegisterStubEvent>(*(gsi.c2gs_stub_proxy_.get()));
    gsi.gs_session_->connect();
    //GsSessionReg::GetSingleton().emplace<InetAddress>(gsi.entity_id, gs_addr);
    g_gs_sesssion.AddGs(request->node_id(), std::move(gsi));
    LOG_INFO << "connect to game server " << gs_addr.toIpPort() << " server id " << request->node_id();
///<<< END WRITING YOUR CODE StartGS
}

void Ms2gwServiceImpl::StopGS(::google::protobuf::RpcController* controller,
    const ms2gw::StopGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StopGS
	/*for (auto e : GsSessionReg::GetSingleton().view<InetAddress>())
	{
		auto& c = GsSessionReg::GetSingleton().get<InetAddress>(e);
		if (c.toIp() != request->ip() ||
			c.port() != request->port())
		{
			continue;
		}
		GsSessionReg::GetSingleton().destroy(e);
		break;
	}*/
///<<< END WRITING YOUR CODE StopGS
}

void Ms2gwServiceImpl::PlayerEnterGS(::google::protobuf::RpcController* controller,
    const ms2gw::PlayerEnterGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerEnterGS
    auto it = g_client_sessions_->find(request->connection_id());
    if (it == g_client_sessions_->end())
    {
        return;
    }
    it->second.gs_node_id_ = request->node_id();
///<<< END WRITING YOUR CODE PlayerEnterGS
}

///<<<rpc end
}// namespace ms2gw
