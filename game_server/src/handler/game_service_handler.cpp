#include "game_service_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE

#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_server.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/gate_node.h"
#include "src/game_logic/player/player_list.h"
#include "src/pb/pbc/service.h"
#include "src/handler/player_service.h"
#include "src/system/player_common_system.h"
#include "src/thread_local/game_thread_local_storage.h"

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
    auto player_id = request->player_id();
    PlayerCommonSystem::RemovePlayerSession(player_id);
    auto p_it = game_tls.player_list().find(player_id);
    if (p_it != game_tls.player_list().end())//已经在线，直接进入,判断是需要发送哪些信息
    {
        EnterGsInfo enter_info;
        enter_info.set_controller_node_id(request->controller_node_id());
        PlayerCommonSystem::EnterGs(p_it->second, enter_info);
        return;
    }
    auto rit = game_tls.async_player_data().emplace(player_id, tls.registry.create());
    if (!rit.second)
    {
        LOG_ERROR << "EnterGs emplace player not found " << player_id;
        return;
    }
    tls.registry.emplace<EnterGsInfo>(rit.first->second).set_controller_node_id(request->controller_node_id());
    game_tls.player_data_redis_system()->AsyncLoad(player_id);//异步加载过程中断开了，怎么处理？

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
    player_request->ParseFromString(request->msg().body());
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
    auto try_player_id = tls.registry.try_get<Guid>(session_it->second);
    if (nullptr == try_player_id)
    {
        LOG_ERROR << "GatePlayerService player not loading";
        return;
    }
    auto pit = game_tls.player_list().find(*try_player_id);
    if (pit == game_tls.player_list().end())
    {
        LOG_ERROR << "GatePlayerService player not found" << *try_player_id;
        return;
    }
    const MessageUniquePtr player_request(service->GetRequestPrototype(method).New());
    player_request->ParseFromString(request->request());
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
    auto& serviceimpl = service_it->second;
    google::protobuf::Service* service = serviceimpl->service();
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_info.method);
    if (nullptr == method)
    {
        LOG_ERROR << "PlayerService method not found " << request->msg().message_id();
        //todo client error;
        return;
    }
    MessageUniquePtr player_request(service->GetRequestPrototype(method).New());
    player_request->ParseFromString(request->msg().body());
    MessageUniquePtr player_response(service->GetResponsePrototype(method).New());
    serviceimpl->CallMethod(method, session_it->second, get_pointer(player_request), get_pointer(player_response));
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

