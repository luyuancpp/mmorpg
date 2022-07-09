#include "gs_service.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE

#include "src/game_logic/game_registry.h"
#include "src/game_server.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/gate_node.h"
#include "src/network/message_system.h"
#include "src/network/session.h"
#include "src/comp/player_list.h"
#include "src/network/server_component.h"
#include "src/pb/pbc/msgmap.h"
#include "src/service/logic/player_service.h"
#include "src/system/player_common_system.h"

#include "c2gw.pb.h"
#include "logic_proto/scene_server_player.pb.h"
#include "component_proto/player_async_comp.pb.h"
#include "component_proto/player_comp.pb.h"

using MessageUnqiuePtr = std::unique_ptr<google::protobuf::Message>;

///<<< END WRITING YOUR CODE

///<<<rpc begin
void GsServiceImpl::EnterGs(::google::protobuf::RpcController* controller,
    const gsservice::EnterGsRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	//连续顶号进入，还在加载中的话继续加载
	auto player_id = request->player_id();
	PlayerCommonSystem::PlayerSessionOffLine(player_id);

	auto p_it = g_players->find(player_id);
	if (p_it != g_players->end())//已经在线，直接进入
	{
		EnterGsInfo enter_info;
		enter_info.set_ms_node_id(request->ms_node_id());
		PlayerCommonSystem::EnterGs(p_it->second, enter_info);
		return;
	}
	auto rit = g_async_player_data.emplace(player_id, EntityPtr());
	if (!rit.second)
	{
		LOG_ERROR << "EnterGs emplace player not found " << player_id;
		return;
	}
	registry.emplace<EnterGsInfo>(rit.first->second).set_ms_node_id(request->ms_node_id());
	g_player_data_redis_system->AsyncLoad(player_id);//异步加载过程中断开了，怎么处理？
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
	auto it = g_players->find(message_extern.player_id());
	if (it == g_players->end())
	{
		LOG_ERROR << "PlayerService player not found " << message_extern.player_id() << ","
			<< request->descriptor()->full_name();
		return;
	}
	auto msg_id = request->msg().msg_id();
	auto sit = g_serviceinfo.find(msg_id);
	if (sit == g_serviceinfo.end())
	{
		LOG_ERROR << "PlayerService msg not found " << message_extern.player_id() << "," << msg_id;
		return;
	}
	auto service_it = g_player_services.find(sit->second.service);
	if (service_it == g_player_services.end())
	{
		LOG_ERROR << "PlayerService service not found " << message_extern.player_id() << "," << msg_id;
		return;
	}
	auto& serviceimpl = service_it->second;
	google::protobuf::Service* service = serviceimpl->service();
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(sit->second.method);
	if (nullptr == method)
	{
		LOG_ERROR << "PlayerService method not found " << msg_id;
		//todo client error;
		return;
	}
	MessageUnqiuePtr player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(request->msg().body());
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
		LOG_ERROR << "GwPlayerService msg not found " << request->msg_id();
		//todo client error;
		return;
	}
	auto& serviceinfo = mit->second;
	auto it = g_player_services.find(serviceinfo.service);
	if (it == g_player_services.end())
	{
		LOG_ERROR << "GwPlayerService service not found " << request->msg_id();
		return;
	}
	google::protobuf::Service* service = it->second->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(serviceinfo.method);
	if (nullptr == method)
	{
		LOG_ERROR << "GwPlayerService service not found " << request->msg_id();
		return;
	}
	auto cit = g_gate_sessions->find(request->session_id());
	if (cit == g_gate_sessions->end())
	{
		LOG_INFO << "GwPlayerService session not found " << request->msg_id();
		return;
	}
	auto try_player_id = registry.try_get<Guid>(cit->second);
	if (nullptr == try_player_id)
	{
		LOG_ERROR << "GwPlayerService player not loading";
		return;
	}
	auto pit = g_players->find(*try_player_id);
	if (pit == g_players->end())
	{
		LOG_ERROR << "GwPlayerService player not found" << *try_player_id;
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
	PlayerCommonSystem::PlayerSessionOffLine(request->player_id());
    auto it = g_players->find(request->player_id());
	if (it == g_players->end())
	{
		return;
	}	
    LeaveSceneParam lp;
    lp.leaver_ = it->second;
    //ScenesSystem::GetSingleton().LeaveScene(lp);
 	g_players->erase(it);//todo  应该是ms 通知过来

///<<< END WRITING YOUR CODE 
}

void GsServiceImpl::GwConnectGs(::google::protobuf::RpcController* controller,
    const gsservice::ConnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	for (auto e : registry.view<RpcServerConnection>())
	{
		auto& conn = registry.get<RpcServerConnection>(e).conn_;
		if (conn->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		auto& gate_node = *registry.emplace<GateNodePtr>(e, std::make_shared<GateNodePtr::element_type>(conn));
		gate_node.node_info_.set_node_id(request->gate_node_id());
		gate_node.node_info_.set_node_type(kGatewayNode);
		g_gate_nodes->emplace(request->gate_node_id(), e);
		LOG_INFO << "GwConnectGs gate node id " << request->gate_node_id();
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
	Send2Player(request->msg(), request->ex().player_id());
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
