#include "game_service.h"
#include "src/network/rpc_msg_route.h"
///<<< BEGIN WRITING YOUR CODE

#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_server.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/gate_node.h"
#include "src/network/message_system.h"
#include "src/network/session.h"
#include "src/game_logic/player/player_list.h"
#include "src/network/server_component.h"
#include "src/pb/pbc/serviceid/service_method_id.h"
#include "src/service/logic_proto/player_service.h"
#include "src/system/player_common_system.h"
#include "src/thread_local/game_thread_local_storage.h"

#include "c2gate.pb.h"
#include "logic_proto/scene_server_player.pb.h"
#include "component_proto/player_async_comp.pb.h"
#include "component_proto/player_comp.pb.h"

using MessageUnqiuePtr = std::unique_ptr<google::protobuf::Message>;

///<<< END WRITING YOUR CODE

///<<<rpc begin
void GameServiceImpl::EnterGs(::google::protobuf::RpcController* controller,
    const ::GameNodeEnterGsRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
        //连续顶号进入，还在加载中的话继续加载
    auto player_id = request->player_id();
    PlayerCommonSystem::RemovePlayereSession(player_id);
    auto p_it = game_tls.player_list().find(player_id);
    if (p_it != game_tls.player_list().end())//已经在线，直接进入,判断是需要发送哪些信息
    {
        EnterGsInfo enter_info;
        enter_info.set_controller_node_id(request->controller_node_id());
        PlayerCommonSystem::EnterGs(p_it->second, enter_info);
        return;
    }
    auto rit = game_tls.async_player_data().emplace(player_id, EntityPtr());
    if (!rit.second)
    {
        LOG_ERROR << "EnterGs emplace player not found " << player_id;
        return;
    }
    tls.registry.emplace<EnterGsInfo>(rit.first->second).set_controller_node_id(request->controller_node_id());
    game_tls.player_data_redis_system()->AsyncLoad(player_id);//异步加载过程中断开了，怎么处理？

///<<< END WRITING YOUR CODE
}

void GameServiceImpl::Send2Player(::google::protobuf::RpcController* controller,
    const ::NodeServiceMessageRequest* request,
    ::NodeServiceMessageResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    auto it = game_tls.player_list().find(request->ex().player_id());
    if (it == game_tls.player_list().end())
    {
        LOG_ERROR << "PlayerService player not found " << request->ex().player_id() << ","
            << request->descriptor()->full_name() << " msgid " << request->msg().service_method_id();
        return;
    }
    auto sit = g_service_method_info.find(request->msg().service_method_id());
    if (sit == g_service_method_info.end())
    {
        LOG_ERROR << "PlayerService msg not found " << request->ex().player_id() << "," << request->msg().service_method_id();
        return;
    }
    auto service_it = g_player_services.find(sit->second.service);
    if (service_it == g_player_services.end())
    {
        LOG_ERROR << "PlayerService service not found " << request->ex().player_id() << "," << request->msg().service_method_id();
        return;
    }
    auto& serviceimpl = service_it->second;
    google::protobuf::Service* service = serviceimpl->service();
    const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(sit->second.method);
    if (nullptr == method)
    {
        LOG_ERROR << "PlayerService method not found " << request->msg().service_method_id();
        //todo client error;
        return;
    }
    MessageUnqiuePtr player_request(service->GetRequestPrototype(method).New());
    player_request->ParseFromString(request->msg().body());
    MessageUnqiuePtr player_response(service->GetResponsePrototype(method).New());
    serviceimpl->CallMethod(method, it->second, get_pointer(player_request), get_pointer(player_response));

///<<< END WRITING YOUR CODE
}

void GameServiceImpl::ClientSend2Player(::google::protobuf::RpcController* controller,
    const ::GameNodeRpcClientRequest* request,
    ::GameNodeRpcClientResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
   // todo player service move to gate check
  
    auto it = g_player_services.find(request->service());
    if (it == g_player_services.end())
    {
        LOG_ERROR << "GatePlayerService service not found " << request->service();
        return;
    }
    google::protobuf::Service* service = it->second->service();
    const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(request->method());
    if (nullptr == method)
    {
        LOG_ERROR << "GatePlayerService method not found " << request->method();
        return;
    }
    auto cit = game_tls.gate_sessions().find(request->session_id());
    if (cit == game_tls.gate_sessions().end())
    {
        LOG_INFO << "GatePlayerService session not found  " << request->service() << "," << request->method() << "," << request->session_id();
        return;
    }
    auto try_player_id = tls.registry.try_get<Guid>(cit->second);
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
    MessageUnqiuePtr player_request(service->GetRequestPrototype(method).New());
    player_request->ParseFromString(request->request());
    MessageUnqiuePtr player_response(service->GetResponsePrototype(method).New());
    it->second->CallMethod(method, pit->second, get_pointer(player_request), get_pointer(player_response));
    response->set_response(player_response->SerializeAsString());
    response->set_service(request->service());
    response->set_method(request->method());
    response->set_id(request->id());
    response->set_session_id(request->session_id());
///<<< END WRITING YOUR CODE
}

void GameServiceImpl::Disconnect(::google::protobuf::RpcController* controller,
    const ::GameNodeDisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
        //异步加载过程中断开了？
    PlayerCommonSystem::RemovePlayereSession(request->player_id());
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

void GameServiceImpl::GateConnectGs(::google::protobuf::RpcController* controller,
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
        auto& gate_node = *tls.registry.emplace<GateNodePtr>(e, std::make_shared<GateNodePtr::element_type>(conn));
        gate_node.node_info_.set_node_id(request->gate_node_id());
        gate_node.node_info_.set_node_type(kGateNode);
        game_tls.gate_node().emplace(request->gate_node_id(), e);
        LOG_INFO << "GateConnectGs gate node id " << request->gate_node_id();
        break;
    }
///<<< END WRITING YOUR CODE
}

void GameServiceImpl::ControllerSend2PlayerViaGs(::google::protobuf::RpcController* controller,
    const ::NodeServiceMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    ::Send2Player(request->msg().service_method_id(), request->msg(), request->ex().player_id());
///<<< END WRITING YOUR CODE
}

void GameServiceImpl::CallPlayer(::google::protobuf::RpcController* controller,
    const ::NodeServiceMessageRequest* request,
    ::NodeServiceMessageResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    auto it = game_tls.player_list().find(request->ex().player_id());
    if (it == game_tls.player_list().end())
    {
        LOG_ERROR << "PlayerService player not found " << request->ex().player_id() << ","
            << request->descriptor()->full_name() << " msgid " << request->msg().service_method_id();
        return;
    }
    auto sit = g_service_method_info.find(request->msg().service_method_id());
    if (sit == g_service_method_info.end())
    {
        LOG_ERROR << "PlayerService msg not found " << request->ex().player_id() << "," << request->msg().service_method_id();
        return;
    }
    auto service_it = g_player_services.find(sit->second.service);
    if (service_it == g_player_services.end())
    {
        LOG_ERROR << "PlayerService service not found " << request->ex().player_id() << "," << request->msg().service_method_id();
        return;
    }
    auto& serviceimpl = service_it->second;
    google::protobuf::Service* service = serviceimpl->service();
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(sit->second.method);
    if (nullptr == method)
    {
        LOG_ERROR << "PlayerService method not found " << request->msg().service_method_id();
        //todo client error;
        return;
    }
    MessageUnqiuePtr player_request(service->GetRequestPrototype(method).New());
    player_request->ParseFromString(request->msg().body());
    MessageUnqiuePtr player_response(service->GetResponsePrototype(method).New());
    serviceimpl->CallMethod(method, it->second, get_pointer(player_request), get_pointer(player_response));
    if (nullptr == response)
    {
        return;
    }
    response->mutable_msg()->set_body(player_response->SerializeAsString());
    response->mutable_ex()->set_player_id(request->ex().player_id());
    response->mutable_msg()->set_service_method_id(request->msg().service_method_id());
///<<< END WRITING YOUR CODE
}

void GameServiceImpl::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
    const ::RouteMsgStringRequest* request,
    ::RouteMsgStringResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GameServiceImpl::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
    const ::RoutePlayerMsgStringRequest* request,
    ::RoutePlayerMsgStringResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<<rpc end
