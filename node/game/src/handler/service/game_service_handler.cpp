#include "game_service_handler.h"
#include "thread_local/storage.h"
#include "system/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/net/InetAddress.h"

#include "game_node.h"
#include "handler/service/player_service.h"
#include "network/gate_session.h"
#include "network/rpc_session.h"
#include "service/service.h"
#include "system/player/player_node.h"
#include "system/player/player_scene.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage_common_logic.h"
#include "thread_local/storage_game.h"
#include "type_alias/player_session.h"
#include "util/defer.h"
#include "util/pb.h"

#include "proto/logic/component/player_async_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

using namespace muduo::net;

///<<< END WRITING YOUR CODE
void GameServiceHandler::EnterGs(::google::protobuf::RpcController* controller,
	const ::GameNodeEnterGsRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    //连续顶号进入，还在加载中的话继续加载
    PlayerNodeSystem::RemovePlayerSession(request->player_id());

    //已经在线，直接进入,判断是需要发送哪些信息
    const auto player_it = tls_cl.player_list().find(request->player_id());
    if (player_it != tls_cl.player_list().end())
    {
        EnterGsInfo enter_info;
        enter_info.set_centre_node_id(request->centre_node_id());
        PlayerNodeSystem::EnterGs(player_it->second, enter_info);
        return;
    }

    //todo 异步加载不了
    EnterGsInfo enter_info;
    enter_info.set_centre_node_id(request->centre_node_id());
    if ( const auto async_player_it = tls_game.async_player_list_.emplace(request->player_id(), enter_info) ;
        !async_player_it.second)
    {
        LOG_ERROR << "EnterGs emplace player  " << request->player_id();
        return;
    }
    //异步加载过程中断开了，怎么处理？
    tls_game.player_redis_->AsyncLoad(request->player_id());
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::Send2Player(::google::protobuf::RpcController* controller,
	const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    const auto it = tls_sessions.find(request->head().session_id());
    if (it == tls_sessions.end())
    {
        LOG_ERROR << "session id not found " << request->head().session_id() << ","
        << " message id " << request->body().message_id();
        return;
    }
    const auto player_it = tls_cl.player_list().find(it->second.player_id());
    if (player_it == tls_cl.player_list().end())
    {
        return;
    }
    const auto player = player_it->second;
	if (request->body().message_id() >= g_message_info.size())
	{
		LOG_ERROR << "message_id not found " << request->body().message_id();
		return;
	}
    const auto& message_info = g_message_info[request->body().message_id()];
    const auto service_it = g_player_service.find(message_info.service);
    if (service_it == g_player_service.end())
    {
        LOG_ERROR << "PlayerService service not found " << request->head().session_id()
        << "," << request->body().message_id();
        return;
    }
    const auto& service_handler = service_it->second;
    google::protobuf::Service* service = service_handler->service();
    const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(message_info.method);
    if (nullptr == method)
    {
        LOG_ERROR << "PlayerService method not found " << request->body().message_id();
        //todo client error;
        return;
    }
    const MessageUniquePtr player_request(service->GetRequestPrototype(method).New());
    if (!player_request->ParsePartialFromArray(request->body().body().data(), int32_t(request->body().body().size())))
    {
        LOG_ERROR << "ParsePartialFromArray " << request->body().message_id();
        return;
    }
    const MessageUniquePtr player_response(service->GetResponsePrototype(method).New());
    service_handler->CallMethod(method, player, get_pointer(player_request), get_pointer(player_response));

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
    entt::entity session{ request->session_id() };
    const auto it = tls_sessions.find(request->session_id());
    if (it == tls_sessions.end())
    {
        LOG_ERROR << "session id not found " << request->session_id() << ","
            << " message id " << request->message_id();
        return;
    }
    const auto player = tls_cl.get_player(it->second.player_id());
    if (entt::null == player)
    {
        LOG_ERROR << "GatePlayerService player not loading " << request->message_id() <<
            "player_id" << it->second.player_id() ;
        return;
    }
    const MessageUniquePtr player_request(service->GetRequestPrototype(method).New());
    player_request->ParseFromArray(request->body().data(), static_cast < int32_t > ( request -> body ( ) . size ( ) ));
    const MessageUniquePtr player_response(service->GetResponsePrototype(method).New());
    service_it->second->CallMethod(method, player, get_pointer(player_request), get_pointer(player_response));
    response->set_response(player_response->SerializeAsString());
    response->set_message_id(request->message_id());
    response->set_id(request->id());
    response->set_session_id(request->session_id());
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::Disconnect(::google::protobuf::RpcController* controller,
	const ::GameNodeDisconnectRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
        //异步加载过程中断开了？
    auto player = tls_cl.get_player(request->player_id());
    defer(tls_cl.player_list().erase(request->player_id()));
    PlayerNodeSystem::RemovePlayerSession(request->player_id());
    Destroy(tls.registry,player);
   //todo  应该是controller 通知过来

///<<< END WRITING YOUR CODE
}

void GameServiceHandler::RegisterGate(::google::protobuf::RpcController* controller,
	const ::RegisterGateRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    const InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
    for (const auto& [e, session] : tls.networkRegistry.view<RpcSession>().each())
    {
        if (session.conn_->peerAddress().toIpPort() != session_addr.toIpPort())
        {
            continue;
        }
        const auto gate_node_id = tls.gateNodeRegistry.create(entt::entity{ request->gate_node_id() });
            tls.gateNodeRegistry.emplace<RpcSessionPtr>(gate_node_id, 
                std::make_shared<RpcSessionPtr::element_type>(session.conn_));
        assert(gate_node_id == entt::entity{ request->gate_node_id() });
        LOG_DEBUG << " gate register: " << MessageToJsonString(request);
        break;
    }
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::CentreSend2PlayerViaGs(::google::protobuf::RpcController* controller,
	const ::NodeRouteMessageRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    const auto it = tls_sessions.find(request->head().session_id());
    if (it == tls_sessions.end())
    {
        LOG_ERROR << "session id not found " << request->head().session_id() << ","
            << " message id " << request->body().message_id();
        return;
    }
    const auto player = tls_cl.get_player(it->second.player_id());
    if (entt::null == player)
    {
        LOG_ERROR << "GatePlayerService player not loading";
        return;
    }
    ::SendToPlayer(request->body().message_id(), request->body(), player);
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::CallPlayer(::google::protobuf::RpcController* controller,
	const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    const auto it = tls_sessions.find(request->head().session_id());
    if (it == tls_sessions.end())
    {
        LOG_ERROR << "session id not found " << request->head().session_id() << ","
            << " message id " << request->body().message_id();
        return;
    }
    auto player = tls_cl.get_player(it->second.player_id());
    if (entt::null == player)
    {
        LOG_ERROR << "GatePlayerService player not loading";
        return;
    }
	if (request->body().message_id() >= g_message_info.size())
	{
		LOG_ERROR << "message_id not found " << request->body().message_id();
		return;
	}
    auto& message_info = g_message_info[request->body().message_id()];
    const auto service_it = g_player_service.find(message_info.service);
    if (service_it == g_player_service.end())
    {
        LOG_ERROR << "PlayerService service not found " << request->head().session_id()
        << "," << request->body().message_id();
        return;
    }
    const auto& service_handler = service_it->second;
    google::protobuf::Service* service = service_handler->service();
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_info.method);
    if (nullptr == method)
    {
        LOG_ERROR << "PlayerService method not found " << request->body().message_id();
        //todo client error;
        return;
    }
    MessageUniquePtr player_request(service->GetRequestPrototype(method).New());
    if (!player_request->ParsePartialFromArray(request->body().body().data(), int32_t(request->body().body().size())))
    {
        LOG_ERROR << "ParsePartialFromArray " << request->body().message_id();
        return;
    }
    MessageUniquePtr player_response(service->GetResponsePrototype(method).New());
    service_handler->CallMethod(method, player, get_pointer(player_request), get_pointer(player_response));
    if (nullptr == response)
    {
        return;
    }
    response->mutable_body()->set_body(player_response->SerializeAsString());
    response->mutable_head()->set_session_id(request->head().session_id());
    response->mutable_body()->set_message_id(request->body().message_id());
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
	const ::RegisterPlayerSessionRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    PlayerNodeSystem::RemovePlayerSession(request->player_id());
    //todo test
if ( const entt::entity gate_node_id{ get_gate_node_id(request->session_id()) } ;
    !tls.gateNodeRegistry.valid(gate_node_id))
    {
        LOG_ERROR << "gate not found " << get_gate_node_id(request->session_id());
        return;
    }

    const auto player = tls_cl.get_player(request->player_id());
    if (!tls.registry.valid(player))
    {
        LOG_ERROR << "player not found " << request->player_id();
        return;
    }

    PlayerSessionInfo session_info;
    session_info.set_player_id(request->player_id());
    tls_sessions.emplace(request->session_id(), session_info);
    if ( auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player) ; nullptr == player_node_info)
    {
        //登录更新gate
        tls.registry.emplace_or_replace<PlayerNodeInfo>(player).set_gate_session_id(request->session_id());
    }
    else
    {
        player_node_info->set_gate_session_id(request->session_id());
    }
    PlayerNodeSystem::OnRegister2GatePlayerGameNode(player);
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::EnterScene(::google::protobuf::RpcController* controller,
	const ::Centre2GsEnterSceneRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    //todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
    PlayerSceneSystem::EnterScene(tls_cl.get_player(request->player_id()), request->scene_id());
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::CreateScene(::google::protobuf::RpcController* controller,
	const ::CreateSceneRequest* request,
	::CreateSceneResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

