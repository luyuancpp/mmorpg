#include "gs_service.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE

#include "src/game_logic/game_registry.h"
#include "src/game_server.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/gate_node.h"
#include "src/network/gate_session.h"
#include "src/network/message_system.h"
#include "src/network/session.h"
#include "src/comp/player_list.h"
#include "src/network/server_component.h"
#include "src/pb/pbc/msgmap.h"
#include "src/service/logic/player_service.h"
#include "src/system/player_reids_system.h"
#include "src/system/player_network_system.h"

#include "c2gw.pb.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/pb/pbc/component_proto/player_async_comp.pb.h"
#include "src/pb/pbc/component_proto/player_comp.pb.h"
#include "src/pb/pbc/component_proto/player_network_comp.pb.h"

using MessageUnqiuePtr = std::unique_ptr<google::protobuf::Message>;

void KickOldSession(Guid player_id)
{
    auto sit = g_player_session_map.find(player_id);
    if (sit == g_player_session_map.end())//老连接踢掉
    {
        //delete old session
		return;
    }
    g_gate_sessions.erase(sit->first);
    g_player_session_map.erase(sit);
}
///<<< END WRITING YOUR CODE

///<<<rpc begin
void GsServiceImpl::EnterGs(::google::protobuf::RpcController* controller,
    const gsservice::EnterGsRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto player_id = request->player_id();
	KickOldSession(player_id);
	g_player_session_map.emplace(player_id, request->session_id());
	auto p_it = g_players.find(player_id);
	if (p_it == g_players.end())
	{
		g_player_data_redis_system->AsyncLoad(player_id);//异步加载过程中断开了，怎么处理？
		EntityPtr session;
		g_gate_sessions.emplace(request->session_id(), session);
		auto& enter_info = registry.emplace<EnterGsInfo>(session);
		enter_info.mutable_scenes_info()->CopyFrom(request->scenes_info());
		enter_info.set_ms_node_id(request->ms_node_id());
	}
	else//在这个gs已经在线了，顶号,不能再去加载了，否则会用旧的数据覆盖内存数据
	{
		PlayerNetworkSystem::EnterGs(p_it->second, request->session_id(), request->ms_node_id());
	}	
///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::PlayerService(::google::protobuf::RpcController* controller,
    const gsservice::MsPlayerMessageRequest* request,
    gsservice::MsPlayerMessageRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto& message_extern = request->ex();
	auto& player_msg = request->msg();
	auto it = g_players.find(message_extern.player_id());
	if (it == g_players.end())
	{
		LOG_INFO << "player not found " << message_extern.player_id();
		return;
	}
	auto msg_id = request->msg().msg_id();
	auto sit = g_serviceinfo.find(msg_id);
	if (sit == g_serviceinfo.end())
	{
		LOG_INFO << "msg not found " << msg_id;
		return;
	}
	auto service_it = g_player_services.find(sit->second.service);
	if (service_it == g_player_services.end())
	{
		LOG_INFO << "msg not found " << msg_id;
		return;
	}
	auto& serviceimpl = service_it->second;
	google::protobuf::Service* service = serviceimpl->service();
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(sit->second.method);
	if (nullptr == method)
	{
		LOG_INFO << "msg not found " << msg_id;
		//todo client error;
		return;
	}
	MessageUnqiuePtr player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(player_msg.body());
	MessageUnqiuePtr player_response(service->GetResponsePrototype(method).New());
	serviceimpl->CallMethod(method, it->second, get_pointer(player_request), get_pointer(player_response));
	if (nullptr == response)//不需要回复
	{
		return;
	}
	response->mutable_ex()->set_player_id(request->ex().player_id());
	response->mutable_msg()->set_body(player_response->SerializeAsString());
	response->mutable_msg()->set_msg_id(msg_id);
///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::PlayerServiceNoRespone(::google::protobuf::RpcController* controller,
    const gsservice::MsPlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::GwPlayerService(::google::protobuf::RpcController* controller,
    const gsservice::RpcClientRequest* request,
    gsservice::RpcClientResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto mit = g_serviceinfo.find(request->msg_id());
	if (mit == g_serviceinfo.end())
	{
		//todo client error;
		return;
	}
	auto& serviceinfo = mit->second;
	auto it = g_player_services.find(serviceinfo.service);
	if (it == g_player_services.end())
	{
		return;
	}
	google::protobuf::Service* service = it->second->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(serviceinfo.method);
	if (nullptr == method)
	{
		return;
	}
	auto cit = g_gate_sessions.find(request->session_id());
	if (cit == g_gate_sessions.end())
	{
		return;
	}
	auto p_try_session_player = registry.try_get<Guid>(cit->second);
	if (nullptr == p_try_session_player)
	{
		LOG_ERROR << "player not loading";
		return;
	}
	auto pit = g_players.find(*p_try_session_player);
	if (pit == g_players.end())
	{
		return;
	}
	MessageUnqiuePtr player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(request->request());
	MessageUnqiuePtr player_response(service->GetResponsePrototype(method).New());
	it->second->CallMethod(method, pit->second, get_pointer(player_request), get_pointer(player_response));
	response->set_response(player_response->SerializeAsString());
///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::Disconnect(::google::protobuf::RpcController* controller,
    const gsservice::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	//异步加载过程中断开了？
	KickOldSession(request->player_id());
    auto it = g_players.find(request->player_id());
	if (it == g_players.end())
	{
		return;
	}	
    LeaveSceneParam lp;
    lp.leaver_ = it->second;
    ScenesSystem::GetSingleton().LeaveScene(lp);
 	g_players.erase(it);//todo  应该是ms 通知过来

///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::GwConnectGs(::google::protobuf::RpcController* controller,
    const gsservice::ConnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
	for (auto e : registry.view<RpcServerConnection>())
	{
		auto& conn = registry.get<RpcServerConnection>(e).conn_;
		if (conn->peerAddress().toIpPort() != rpc_client_peer_addr.toIpPort())
		{
			continue;
		}
		auto& gate_node = *registry.emplace<GateNodePtr>(e, std::make_shared<GateNode>(conn));
		gate_node.node_info_.set_node_id(request->gate_node_id());
		gate_node.node_info_.set_node_type(kGateWayNode);
		g_gate_nodes.emplace(request->gate_node_id(), e);
		LOG_INFO << "gate node id " << request->gate_node_id();
		break;
	}
///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::MsSend2PlayerViaGs(::google::protobuf::RpcController* controller,
    const gsservice::MsSend2PlayerViaGsRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
