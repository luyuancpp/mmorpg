#include "gate_service_handler.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/network/game_node.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/gate_server.h"
#include "src/game_logic/tips_id.h"
#include "src/network/rpc_msg_route.h"
#include "src/thread_local/gate_thread_local_storage.h"

#include "component_proto/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE
void GateServiceHandler::StartGS(::google::protobuf::RpcController* controller,
	const ::GateNodeStartGSRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	InetAddress gs_addr(request->ip(), request->port());
	entt::entity gs = entt::null;
	for (const auto e : tls.registry.view<InetAddress>())
	{
		if (auto& c = tls.registry.get<InetAddress>(e); gs_addr.toIpPort() == c.toIpPort())// to do node id，已经连接过了
		{
			return;
		}
		gs = e;
	}
	GameNode game_node;
	game_node.node_info_.set_node_id(request->game_node_id());
	game_node.node_info_.set_node_type(kGameNode);
	game_node.gs_session_ = std::make_unique<RpcClient>(EventLoop::getEventLoopOfCurrentThread(), gs_addr);
	game_node.gs_session_->registerService(&g_gate_node->gate_service_hanlder());
	tls.registry.emplace<InetAddress>(gs, gs_addr);
	game_node.gs_session_->connect();
	gate_tls.game_nodes().emplace(request->game_node_id(), std::move(game_node));
	LOG_INFO << "connect to game server " << gs_addr.toIpPort() << " server id " << request->game_node_id();
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::StopGS(::google::protobuf::RpcController* controller,
	const ::GateNodeStopGSRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	for (auto e : tls.registry.view<InetAddress>())
	{
		auto& c = tls.registry.get<InetAddress>(e);
		if (c.toIp() != request->ip() ||
			c.port() != request->port())
		{
			continue;
		}
		tls.registry.destroy(e);
		break;
	}
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::PlayerEnterGs(::google::protobuf::RpcController* controller,
	const ::GateNodePlayerUpdateGameNodeRequest* request,
	::GateNodePlayerUpdateGameNodeResponese* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const auto it = gate_tls.sessions().find(request->session_id());
	if (it == gate_tls.sessions().end())
	{
		LOG_ERROR << "conn id not found   " << request->session_id();
		return;
	}
	//注意这里gs发过来的时候可能有异步问题，所以gate更新完gs以后才能告诉controller 让ms去通知gs去发送信息
	it->second.game_node_id_ = request->game_node_id();
	response->set_session_id(request->session_id());
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::PlayerMessage(::google::protobuf::RpcController* controller,
	const ::NodeServiceMessageRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const auto it = gate_tls.sessions().find(request->ex().session_id());
	if (it == gate_tls.sessions().end())
	{
		LOG_ERROR << "session id not found  player id " << request->ex().session_id();
		return;
	}
	g_gate_node->Send2Client(it->second.conn_, request->msg());
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::KickConnByController(::google::protobuf::RpcController* controller,
	const ::GateNodeKickConnRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	gate_tls.sessions().erase(request->session_id());
	LOG_INFO << "conn id be kick " << request->session_id();
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
	const ::RouteMsgStringRequest* request,
	::RouteMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
	const ::RoutePlayerMsgStringRequest* request,
	::RoutePlayerMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}

