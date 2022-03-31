#include "gw_node.h"
#include "src/server_common/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/network/gs_node.h"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/entity/entity.h"
#include "src/gate_player/gate_player_list.h"
#include "src/gateway_server.h"
#include "src/return_code/error_code.h"
#include "src/server_common/rpc_closure.h"

using namespace gateway;
///<<< END WRITING YOUR CODE

using namespace common;
namespace gwservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void GwNodeServiceImpl::StartGS(::google::protobuf::RpcController* controller,
    const gwservice::StartGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartGS
	InetAddress gs_addr(request->ip(), request->port());
	for (auto e : reg.view<InetAddress>())
	{
		auto& c = reg.get<InetAddress>(e);
		if (gs_addr.toIpPort() == c.toIpPort())// to do node id，已经连接过了
		{
			return;
		}
	}
	GsNode gsi;
	gsi.node_info_.set_node_id(request->gs_node_id());
	gsi.node_info_.set_node_type(GAME_SERVER_NODE_TYPE);
	gsi.gs_session_ = std::make_unique<RpcClient>(EventLoop::getEventLoopOfCurrentThread(), gs_addr);
	gsi.gs_stub_ = std::make_unique<RpcStub<gsservice::GsService_Stub>>();
	gsi.gs_session_->subscribe<RegisterStubEvent>(*(gsi.gs_stub_.get()));
	gsi.gs_session_->subscribe<OnConnected2ServerEvent>(*g_gateway_server);
	gsi.gs_session_->registerService(&g_gateway_server->node_service_impl());
	gsi.gs_session_->connect();
	reg.emplace<InetAddress>(gsi.entity_id.entity(), gs_addr);
	g_gs_nodes.emplace(request->gs_node_id(), std::move(gsi));
	LOG_INFO << "connect to game server " << gs_addr.toIpPort() << " server id " << request->gs_node_id();
///<<< END WRITING YOUR CODE StartGS
}

void GwNodeServiceImpl::StopGS(::google::protobuf::RpcController* controller,
    const gwservice::StopGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StopGS
	for (auto e : reg.view<InetAddress>())
	{
		auto& c = reg.get<InetAddress>(e);
		if (c.toIp() != request->ip() ||
			c.port() != request->port())
		{
			continue;
		}
		reg.destroy(e);
		break;
	}
///<<< END WRITING YOUR CODE StopGS
}

void GwNodeServiceImpl::PlayerEnterGS(::google::protobuf::RpcController* controller,
    const gwservice::PlayerEnterGSRequest* request,
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

void GwNodeServiceImpl::PlayerMessage(::google::protobuf::RpcController* controller,
    const gwservice::PlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerMessage
	auto conn_id = request->ex().conn_id();
	auto it = g_client_sessions_->find(conn_id);
	if (it == g_client_sessions_->end())
	{
		LOG_ERROR << "connid not found  player id " << request->ex().player_id() << "," << conn_id;
		return;
	}
	g_gateway_server->Send2Client(it->second.conn_, request->msg());
///<<< END WRITING YOUR CODE PlayerMessage
}

void GwNodeServiceImpl::GsPlayerService(::google::protobuf::RpcController* controller,
    const gwservice::GsPlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE GsPlayerService
	auto conn_id = request->ex().conn_id();
	auto it = g_client_sessions_->find(conn_id);
	if (it == g_client_sessions_->end())
	{
		LOG_ERROR << "connid not found  conn id " << conn_id;
		return;
	}
	g_gateway_server->Send2Client(it->second.conn_, request->msg());
///<<< END WRITING YOUR CODE GsPlayerService
}

///<<<rpc end
}// namespace gwservice
