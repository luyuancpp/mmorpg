#include "gate_service_handler.h"
#include "thread_local/thread_local_storage.h"
#include "network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "thread_local/thread_local_storage.h"
#include "gate_node.h"
#include "network/gate_session.h"
#include "network/rpc_msg_route.h"
#include "thread_local/thread_local_storage_gate.h"
#include "util/pb_util.h"

#include "component_proto/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE
void GateServiceHandler::RegisterGame(::google::protobuf::RpcController* controller,
	const ::RegisterGameRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	//centre服务器通知过来
	entt::entity request_game_node_id{ request->game_node_id() };
	if (tls.game_node_registry.valid(request_game_node_id))
	{
        LOG_ERROR << " game node reconnect";
		return;
	}
	Destroy(tls.game_node_registry, request_game_node_id);
	auto game_node_id = tls.game_node_registry.create(request_game_node_id);
	if (game_node_id != request_game_node_id)
	{
		LOG_ERROR << "create game node ";
		return;
	}
    InetAddress game_servcie_addr(request->rpc_server().ip(), request->rpc_server().port());
    auto& game_node = tls.game_node_registry.emplace<RpcClientPtr>(game_node_id,
        std::make_unique<RpcClientPtr::element_type>(
            EventLoop::getEventLoopOfCurrentThread(),
            game_servcie_addr));
	game_node->registerService(&g_gate_node->gate_service_hanlder());
	game_node->connect();

	LOG_INFO << "on  game register" << MessageToJsonString(request);
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::UnRegisterGame(::google::protobuf::RpcController* controller,
	const ::UnRegisterGameRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
    entt::entity request_game_node_id{ request->game_node_id() };
    Destroy(tls.game_node_registry, request_game_node_id);
    LOG_INFO << "on  game unregister" << MessageToJsonString(request);
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::PlayerEnterGs(::google::protobuf::RpcController* controller,
	const ::RegisterSessionGameNodeRequest* request,
	::RegisterSessionGameNodeResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	auto it = tls_gate.sessions().find(request->session_info().session_id());
	if (it == tls_gate.sessions().end())
	{
		return;
	}
	auto session = it->second;
	//注意这里gs发过来的时候可能有异步问题，所以gate更新完gs以后才能告诉controller 让ms去通知gs去发送信息
	session.game_node_id_ = request->game_node_id();
	response->mutable_session_info()->set_session_id(request->session_info().session_id());
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::PlayerMessage(::google::protobuf::RpcController* controller,
	const ::NodeRouteMessageRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	auto it = tls_gate.sessions().find(request->head().session_id());
    if (it == tls_gate.sessions().end())
    {
        LOG_ERROR << "conn id not found  session id " << "," << request->head().session_id();
        return;
    }
	g_gate_node->Send2Client(it->second.conn_, request->body());
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::KickConnByCentre(::google::protobuf::RpcController* controller,
	const ::GateNodeKickConnRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	Destroy(tls.scene_registry, entt::entity{request->session_id()});
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

