#include "ms2gw.h"

///<<< BEGIN WRITING YOUR CODE
#include "src/gs/gs_session.h"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/entity/entity.h"
#include "src/gate_player/gate_player_list.h"
#include "src/gateway_server.h"
#include "src/return_code/error_code.h"
#include "src/server_common/closure_auto_done.h"

#include "gw2gs.pb.h"
#include "c2gw.pb.h"

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
    GsNode gsi;
    gsi.node_info_.set_node_id(request->gs_node_id());
    gsi.node_info_.set_node_type(GAME_SERVER_NODTE_TYPE);
    gsi.gs_session_ = std::make_unique<RpcClient>(EventLoop::getEventLoopOfCurrentThread(), gs_addr);
    gsi.gw2gs_stub_ = std::make_unique<RpcStub<gw2gs::Gw2gsService_Stub>>();
    gsi.gs_session_->subscribe<RegisterStubEvent>(*(gsi.gw2gs_stub_.get()));
    gsi.gs_session_->connect();
    reg.emplace<InetAddress>(gsi.entity_id.entity(), gs_addr);
    g_gs_nodes.AddGs(request->gs_node_id(), std::move(gsi));
    LOG_INFO << "connect to game server " << gs_addr.toIpPort() << " server id " << request->gs_node_id();
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
    auto it = g_client_sessions_->find(request->conn_id());
    if (it == g_client_sessions_->end())
    {
        LOG_INFO << "connid not found  player id " << request->player_id() << "," << request->conn_id();
        return;
    }
    it->second.guid_ = request->player_id();
    it->second.gs_node_id_ = request->gs_node_id();
    c2gw::EnterGameResponse message;
    message.mutable_error()->set_error_no(RET_OK);
    g_gateway_server->Send2Client(it->second.conn_, message);

///<<< END WRITING YOUR CODE PlayerEnterGS
}

void Ms2gwServiceImpl::PlayerMessage(::google::protobuf::RpcController* controller,
    const ms2gw::Ms2PlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerMessage
    auto conn_id = request->request_extern().conn_id();
	auto it = g_client_sessions_->find(conn_id);
	if (it == g_client_sessions_->end())
	{
		LOG_ERROR << "connid not found  player id " << request->request_extern().player_id() << "," << conn_id;
		return;
	}
	g_gateway_server->Send2Client(it->second.conn_, request->player_message());
///<<< END WRITING YOUR CODE PlayerMessage
}

///<<<rpc end
}// namespace ms2gw
