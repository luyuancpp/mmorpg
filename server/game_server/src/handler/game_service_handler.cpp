#include "game_service_handler.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
///
#include "src/game_server.h"
#include "src/system/scene/scene_system.h"
#include "src/network/gate_node.h"
#include "src/thread_local/common_logic_thread_local_storage.h"
#include "service/service.h"
#include "src/network/session.h"
#include "src/handler/player_service.h"
#include "src/system/player_common_system.h"
#include "src/thread_local/game_thread_local_storage.h"
#include "src/system/player_scene_system.h"
#include "component_proto/player_network_comp.pb.h"

#include "component_proto/player_async_comp.pb.h"

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

///<<< END WRITING YOUR CODE
void GameServiceHandler::EnterGs(::google::protobuf::RpcController* controller,
	const ::GameNodeEnterGsRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    //连续顶号进入，还在加载中的话继续加载
    PlayerCommonSystem::RemovePlayerSession(request->player_id());
    //已经在线，直接进入,判断是需要发送哪些信息
    if (const auto player_it = game_tls.player_list().find(request->player_id());
        player_it != game_tls.player_list().end())
    {
        EnterGsInfo enter_info;
        enter_info.set_controller_node_id(request->controller_node_id());
        PlayerCommonSystem::EnterGs(player_it->second, enter_info);
        return;
    }
    const auto player_it = game_tls.async_player_data().emplace(request->player_id(), tls.registry.create());
    if (!player_it.second)
    {
        LOG_ERROR << "EnterGs emplace player  " << request->player_id();
        return;
    }
    tls.registry.emplace<EnterGsInfo>(player_it.first->second).set_controller_node_id(request->controller_node_id());
    //异步加载过程中断开了，怎么处理？
    game_tls.player_data_redis_system()->AsyncLoad(request->player_id());

///<<< END WRITING YOUR CODE
}

void GameServiceHandler::Send2Player(::google::protobuf::RpcController* controller,
	const ::NodeServiceMessageRequest* request,
	::NodeServiceMessageResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    const auto session_it = game_tls.gate_sessions().find(request->ex().session_id());
    if (session_it == game_tls.gate_sessions().end())
    {
        LOG_INFO << "session id not found " << request->ex().session_id() << ","
        << " message id " << request->msg().message_id();
        return;
    }
	if (request->msg().message_id() >= g_message_info.size())
	{
		LOG_ERROR << "message_id not found " << request->msg().message_id();
		return;
	}
    const auto& message_info = g_message_info[request->msg().message_id()];
    const auto service_it = g_player_service.find(message_info.service);
    if (service_it == g_player_service.end())
    {
        LOG_ERROR << "PlayerService service not found " << request->ex().session_id()
        << "," << request->msg().message_id();
        return;
    }
    const auto& service_handler = service_it->second;
    google::protobuf::Service* service = service_handler->service();
    const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(message_info.method);
    if (nullptr == method)
    {
        LOG_ERROR << "PlayerService method not found " << request->msg().message_id();
        //todo client error;
        return;
    }
    const MessageUniquePtr player_request(service->GetRequestPrototype(method).New());
    if (!player_request->ParsePartialFromArray(request->msg().body().data(), int32_t(request->msg().body().size())))
    {
        LOG_ERROR << "ParsePartialFromArray " << request->msg().message_id();
        return;
    }
    const MessageUniquePtr player_response(service->GetResponsePrototype(method).New());
    service_handler->CallMethod(method, session_it->second, get_pointer(player_request), get_pointer(player_response));

///<<< END WRITING YOUR CODE
}

void GameServiceHandler::ClientSend2Player(::google::protobuf::RpcController* controller,
	const ::GameNodeRpcClientRequest* request,
	::GameNodeRpcClientResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
   // todo player service move to gate check
    if (request->message_id() >= g_message_info.size())
    {
        LOG_ERROR << "message_id not found " << request->message_id();
        return;
    }
    const auto& message_info = g_message_info.at(request->message_id());
    const auto service_it = g_player_service.find(message_info.service);
    if (service_it == g_player_service.end())
    {
        LOG_ERROR << "GatePlayerService message id not found " << request->message_id();
        return;
    }
    google::protobuf::Service* service = service_it->second->service();
    const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(message_info.method);
    if (nullptr == method)
    {
        LOG_ERROR << "GatePlayerService message id not found " << request->message_id();
        return;
    }
    auto session_it = game_tls.gate_sessions().find(request->session_id());
    if (session_it == game_tls.gate_sessions().end())
    {
        LOG_INFO << "GatePlayerService session not found  " << request->message_id() << "," << request->session_id();
        return;
    }
    auto session_player_id = tls.registry.try_get<Guid>(session_it->second);
    if (nullptr == session_player_id)
    {
        LOG_ERROR << "GatePlayerService player not loading";
        return;
    }
    auto pit = game_tls.player_list().find(*session_player_id);
    if (pit == game_tls.player_list().end())
    {
        LOG_ERROR << "GatePlayerService player not found" << *session_player_id;
        return;
    }
    const MessageUniquePtr player_request(service->GetRequestPrototype(method).New());
    player_request->ParseFromArray(request->request().data(), int32_t(request->request().size()));
    const MessageUniquePtr player_response(service->GetResponsePrototype(method).New());
    service_it->second->CallMethod(method, pit->second, get_pointer(player_request), get_pointer(player_response));
    response->set_response(player_response->SerializeAsString());
    response->set_message_id(request->message_id());
    response->set_id(request->id());
    response->set_session_id(request->session_id());
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::Disconnect(::google::protobuf::RpcController* controller,
	const ::GameNodeDisconnectRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
        //异步加载过程中断开了？
    PlayerCommonSystem::RemovePlayerSession(request->player_id());
    auto it = game_tls.player_list().find(request->player_id());
    if (it == game_tls.player_list().end())
    {
        return;
    }
    LeaveSceneParam lp;
    lp.leaver_ = it->second;
    //ScenesSystem::LeaveScene(lp);
    game_tls.player_list().erase(it);//todo  应该是controller 通知过来

///<<< END WRITING YOUR CODE
}

void GameServiceHandler::GateConnectGs(::google::protobuf::RpcController* controller,
	const ::GameNodeConnectRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
    for (auto e : tls.registry.view<RpcServerConnection>())
    {
        auto& conn = tls.registry.get<RpcServerConnection>(e).conn_;
        if (conn->peerAddress().toIpPort() != session_addr.toIpPort())
        {
            continue;
        }
        auto gate_node = std::make_shared<GateNodePtr::element_type>(conn);
        gate_node->node_info_.set_node_id(request->gate_node_id());
        gate_node->node_info_.set_node_type(kGateNode);
        game_tls.gate_node().emplace(request->gate_node_id(), gate_node);
        LOG_INFO << "GateConnectGs gate node id " << request->gate_node_id();
        break;
    }
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::ControllerSend2PlayerViaGs(::google::protobuf::RpcController* controller,
	const ::NodeServiceMessageRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    const auto session_it = game_tls.gate_sessions().find(request->ex().session_id());
    if (session_it == game_tls.gate_sessions().end())
    {
        LOG_INFO << "session id not found " << request->ex().session_id() << ","
        << " message id " << request->msg().message_id();
        return;
    }
    ::Send2Player(request->msg().message_id(), request->msg(), session_it->second);
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::CallPlayer(::google::protobuf::RpcController* controller,
	const ::NodeServiceMessageRequest* request,
	::NodeServiceMessageResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    const auto session_it = game_tls.gate_sessions().find(request->ex().session_id());
    if (session_it == game_tls.gate_sessions().end())
    {
        LOG_INFO << "session id not found " << request->ex().session_id() << ","
        << " message id " << request->msg().message_id();
        return;
    }
	if (request->msg().message_id() >= g_message_info.size())
	{
		LOG_ERROR << "message_id not found " << request->msg().message_id();
		return;
	}
    auto& message_info = g_message_info[request->msg().message_id()];
    auto service_it = g_player_service.find(message_info.service);
    if (service_it == g_player_service.end())
    {
        LOG_ERROR << "PlayerService service not found " << request->ex().session_id()
        << "," << request->msg().message_id();
        return;
    }
    auto& service_handler = service_it->second;
    google::protobuf::Service* service = service_handler->service();
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_info.method);
    if (nullptr == method)
    {
        LOG_ERROR << "PlayerService method not found " << request->msg().message_id();
        //todo client error;
        return;
    }
    MessageUniquePtr player_request(service->GetRequestPrototype(method).New());
    if (!player_request->ParsePartialFromArray(request->msg().body().data(), int32_t(request->msg().body().size())))
    {
        LOG_ERROR << "ParsePartialFromArray " << request->msg().message_id();
        return;
    }
    MessageUniquePtr player_response(service->GetResponsePrototype(method).New());
    service_handler->CallMethod(method, session_it->second, get_pointer(player_request), get_pointer(player_response));
    if (nullptr == response)
    {
        return;
    }
    response->mutable_msg()->set_body(player_response->SerializeAsString());
    response->mutable_ex()->set_session_id(request->ex().session_id());
    response->mutable_msg()->set_message_id(request->msg().message_id());
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
	const ::RouteMsgStringRequest* request,
	::RouteMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
	const ::RoutePlayerMsgStringRequest* request,
	::RoutePlayerMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::UpdateSession(::google::protobuf::RpcController* controller,
	const ::UpdatePlayerSessionRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    PlayerCommonSystem::RemovePlayerSession(request->player_id());
    const auto gate_node_id = get_gate_node_id(request->session_id());
    //todo test
    if (const auto gate_it = game_tls.gate_node().find(gate_node_id);
        gate_it == game_tls.gate_node().end())
    {
        LOG_ERROR << "gate not found " << gate_node_id;
        return;
    }
    const auto player_it = game_tls.player_list().find(request->player_id());
    if (player_it == game_tls.player_list().end())
    {
        LOG_ERROR << "player not found " << request->player_id();
        return;
    }

    game_tls.gate_sessions().emplace(request->session_id(), player_it->second);
    auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player_it->second);
    if (nullptr == player_node_info)
    {
        //登录更新gate
        tls.registry.emplace_or_replace<PlayerNodeInfo>(player_it->second).set_gate_session_id(request->session_id());
    }
    else
    {
        player_node_info->set_gate_session_id(request->session_id());
    }
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::EnterScene(::google::protobuf::RpcController* controller,
	const ::Ctlr2GsEnterSceneRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    const auto player_it = game_tls.player_list().find(request->player_id());
    if (player_it == game_tls.player_list().end())
    {
        LOG_INFO << "player id not found " << request->player_id() ;
        return;
    }
    //todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
    PlayerSceneSystem::EnterScene(player_it->second, request->scene_id());
///<<< END WRITING YOUR CODE
}

