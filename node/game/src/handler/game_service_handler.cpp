#include "game_service_handler.h"
#include "thread_local/thread_local_storage.h"
#include "network/message_system.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/net/InetAddress.h"

#include "game_node.h"
#include "system/scene/scene_system.h"
#include "thread_local/thread_local_storage_common_logic.h"
#include "service/service.h"
#include "network/gate_session.h"
#include "network/rpc_session.h"
#include "handler/player_service.h"
#include "system/player_common_system.h"
#include "thread_local/game_thread_local_storage.h"
#include "thread_local/thread_local_storage_common_logic.h"
#include "system/player_scene_system.h"
#include "util/defer.h"
#include "util/pb_util.h"

#include "component_proto/player_network_comp.pb.h"
#include "component_proto/player_async_comp.pb.h"

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
    PlayerCommonSystem::RemovePlayerSession(request->player_id());
    //已经在线，直接进入,判断是需要发送哪些信息
    auto player_it = cl_tls.player_list().find(request->player_id());
    if (player_it != cl_tls.player_list().end())
    {
        EnterGsInfo enter_info;
        enter_info.set_centre_node_id(request->centre_node_id());
        PlayerCommonSystem::EnterGs(player_it->second, enter_info);
        return;
    }

    //todo 异步加载不了
    EnterGsInfo enter_info;
    enter_info.set_centre_node_id(request->centre_node_id());
    const auto async_player_it = game_tls.async_player_data().emplace(request->player_id(), enter_info);
    if (!async_player_it.second)
    {
        LOG_ERROR << "EnterGs emplace player  " << request->player_id();
        return;
    }
    //异步加载过程中断开了，怎么处理？
    game_tls.player_data_redis_system()->AsyncLoad(request->player_id());

///<<< END WRITING YOUR CODE
}

void GameServiceHandler::Send2Player(::google::protobuf::RpcController* controller,
	const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    entt::entity sesion_id{ request->ex().session_id()};
    if (!tls.session_registry.valid(sesion_id))
    {
        LOG_INFO << "session id not found " << request->ex().session_id() << ","
        << " message id " << request->msg().message_id();
        return;
    }
    auto player_guid = tls.session_registry.try_get<Guid>(sesion_id);
    if (nullptr == player_guid)
    {
        LOG_INFO << "session player id not found " << request->ex().session_id() << ","
            << " message id " << request->msg().message_id();
        return;
    }
    
    auto player_it = cl_tls.player_list().find(*player_guid);
    if (player_it == cl_tls.player_list().end())
    {
        return;
    }
    auto player = player_it->second;
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
    if (tls.session_registry.valid(session))
    {
        LOG_INFO << "GatePlayerService session not found  " << request->message_id() << "," << request->session_id();
        return;
    }
    auto player_guid = tls.session_registry.try_get<Guid>(session);
    if (nullptr == player_guid )
    {
        LOG_ERROR << "GatePlayerService player not loading";
        return;
    }
    auto player = cl_tls.get_player(*player_guid );
    if (entt::null == player)
    {
        LOG_ERROR << "GatePlayerService player not loading";
        return;
    }
    const MessageUniquePtr player_request(service->GetRequestPrototype(method).New());
    player_request->ParseFromArray(request->body().data(), int32_t(request->body().size()));
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
    auto player = cl_tls.get_player(request->player_id());
    defer(cl_tls.player_list().erase(request->player_id()));
    PlayerCommonSystem::RemovePlayerSession(request->player_id());
    LeaveSceneParam lp;
    lp.leaver_ = player;
    //ScenesSystem::LeaveScene(lp);
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
    InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
    for (auto e : tls.network_registry.view<RpcSession>())
    {
        auto& conn = tls.network_registry.get<RpcSession>(e).conn_;
        if (conn->peerAddress().toIpPort() != session_addr.toIpPort())
        {
            continue;
        }
        auto gate_node_id = tls.gate_node_registry.create(entt::entity{ request->gate_node_id() });
        auto& gate_node =
            tls.gate_node_registry.emplace<RpcSessionPtr>(gate_node_id, 
                std::make_shared<RpcSessionPtr::element_type>(conn));
        assert(gate_node_id == entt::entity{ request->gate_node_id() });
        LOG_INFO << " gate register: " << MessageToJsonString(request);
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
    entt::entity session_id{ request->ex().session_id() };
    if (!tls.session_registry.valid(session_id))
    {
        LOG_INFO << "session id not found " << request->ex().session_id() << ","
            << " message id " << request->msg().message_id();
        return;
    }
    auto player_guid = tls.session_registry.try_get<Guid>(session_id);
    if (nullptr == player_guid)
    {
        LOG_ERROR << "GatePlayerService player not loading";
        return;
    }
    auto player = cl_tls.get_player(*player_guid);
    if (entt::null == player)
    {
        LOG_ERROR << "GatePlayerService player not loading";
        return;
    }
    ::Send2Player(request->msg().message_id(), request->msg(), player);
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::CallPlayer(::google::protobuf::RpcController* controller,
	const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    entt::entity session_id{ request->ex().session_id() };
    if (tls.session_registry.valid(session_id))
    {
        LOG_INFO << "session id not found " << request->ex().session_id() << ","
        << " message id " << request->msg().message_id();
        return;
    }
    auto player_guid = tls.session_registry.try_get<Guid>(session_id);
    if (nullptr == player_guid)
    {
        return;
    }
    auto player = cl_tls.get_player(*player_guid);
    if (!tls.registry.valid(player))
    {
        LOG_ERROR << "player not found" << *player_guid;
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
    service_handler->CallMethod(method, player, get_pointer(player_request), get_pointer(player_response));
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
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    PlayerCommonSystem::RemovePlayerSession(request->player_id());
    //todo test
    entt::entity gate_node_id{ get_gate_node_id(request->session_id()) };
    if (!tls.gate_node_registry.valid(gate_node_id))
    {
        LOG_ERROR << "gate not found " << get_gate_node_id(request->session_id());
        return;
    }

    auto player = cl_tls.get_player(request->player_id());
    if (tls.registry.valid(player))
    {
        LOG_ERROR << "player not found " << request->player_id();
        return;
    }

    entt::entity session_id{ request->session_id() };
    auto create_session_id = tls.session_registry.create(session_id);
    if (create_session_id != session_id)
    {
        Destroy(tls.session_registry, create_session_id);
        LOG_ERROR << "session create " << request->player_id();

        return;
    }
    tls.session_registry.emplace<Guid>(session_id, entt::to_integral(player));

    auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
    if (nullptr == player_node_info)
    {
        //登录更新gate
        tls.registry.emplace_or_replace<PlayerNodeInfo>(player).set_gate_session_id(request->session_id());
    }
    else
    {
        player_node_info->set_gate_session_id(request->session_id());
    }
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::EnterScene(::google::protobuf::RpcController* controller,
	const ::Ctlr2GsEnterSceneRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    //todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
    PlayerSceneSystem::EnterScene(
        cl_tls.get_player(request->player_id()),
        request->scene_id());
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

