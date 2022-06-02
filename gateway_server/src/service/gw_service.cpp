#include "gw_service.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/network/gs_node.h"
#include "src/game_logic/game_registry.h"
#include "src/network/gate_player_list.h"
#include "src/gateway_server.h"
#include "src/game_logic/tips_id.h"
#include "src/network/rpc_closure.h"

#include "component_proto/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void GwNodeServiceImpl::StartGS(::google::protobuf::RpcController* controller,
    const gwservice::StartGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	InetAddress gs_addr(request->ip(), request->port());
	for (auto e : registry.view<InetAddress>())
	{
		auto& c = registry.get<InetAddress>(e);
		if (gs_addr.toIpPort() == c.toIpPort())// to do node id，已经连接过了
		{
			return;
		}
	}
	GsNode gsi;
	gsi.node_info_.set_node_id(request->gs_node_id());
	gsi.node_info_.set_node_type(kGsNode);
	gsi.gs_session_ = std::make_unique<RpcClient>(EventLoop::getEventLoopOfCurrentThread(), gs_addr);
	gsi.gs_stub_ = std::make_unique<RpcStub<gsservice::GsService_Stub>>();
	gsi.gs_session_->subscribe<RegisterStubEvent>(*(gsi.gs_stub_.get()));
	gsi.gs_session_->subscribe<OnConnected2ServerEvent>(*g_gateway_server);
	gsi.gs_session_->registerService(&g_gateway_server->node_service_impl());
	gsi.gs_session_->connect();
	registry.emplace<InetAddress>(gsi.entity_id, gs_addr);
	g_gs_nodes.emplace(request->gs_node_id(), std::move(gsi));
	LOG_INFO << "connect to game server " << gs_addr.toIpPort() << " server id " << request->gs_node_id();
///<<< END WRITING YOUR CODE 
}

void GwNodeServiceImpl::StopGS(::google::protobuf::RpcController* controller,
    const gwservice::StopGSRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	for (auto e : registry.view<InetAddress>())
	{
		auto& c = registry.get<InetAddress>(e);
		if (c.toIp() != request->ip() ||
			c.port() != request->port())
		{
			continue;
		}
		registry.destroy(e);
		break;
	}
///<<< END WRITING YOUR CODE 
}

void GwNodeServiceImpl::PlayerEnterGs(::google::protobuf::RpcController* controller,
    const gwservice::PlayerEnterGsRequest* request,
    gwservice::PlayerEnterGsResponese* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 

	auto it = g_client_sessions_->find(request->session_id());
	if (it == g_client_sessions_->end())
	{
		LOG_INFO << "connid not found   " << request->session_id();
		return;
	}
	it->second.gs_node_id_ = request->gs_node_id();//注意这里gs发过来的时候可能有异步问题，所以gate更新完gs以后才能告诉ms 让ms去通知gs去发送信息

///<<< END WRITING YOUR CODE 
}

void GwNodeServiceImpl::PlayerMessage(::google::protobuf::RpcController* controller,
    const gwservice::PlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto session_id = request->ex().session_id();
	auto it = g_client_sessions_->find(session_id);
	if (it == g_client_sessions_->end())
	{
		LOG_ERROR << "connid not found  player id " << request->ex().player_id() << "," << session_id;
		return;
	}
	g_gateway_server->Send2Client(it->second.conn_, request->msg());
///<<< END WRITING YOUR CODE 
}

void GwNodeServiceImpl::GsPlayerService(::google::protobuf::RpcController* controller,
    const gwservice::GsPlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto session_id = request->ex().session_id();
	auto it = g_client_sessions_->find(session_id);
	if (it == g_client_sessions_->end())
	{
		LOG_ERROR << "connid not found  conn id " << session_id;
		return;
	}
	g_gateway_server->Send2Client(it->second.conn_, request->msg());
///<<< END WRITING YOUR CODE 
}

void GwNodeServiceImpl::KickConnByMs(::google::protobuf::RpcController* controller,
    const gwservice::KickConnRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	g_client_sessions_->erase(request->session_id());
	LOG_INFO << "connid be kick " << request->session_id();
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
