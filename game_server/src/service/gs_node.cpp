#include "gs_node.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE

#include "src/game_logic/comp/player_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/game_server.h"
#include "src/network/gate_node.h"
#include "src/network/message_sys.h"
#include "src/module/player_list/player_list.h"
#include "src/network/server_component.h"
#include "src/pb/pbc/msgmap.h"
#include "src/service/logic/player_service.h"

#include "c2gw.pb.h"
#include "logic_proto/scene_server_player.pb.h"
#include "component_proto/player_comp.pb.h"

///<<< END WRITING YOUR CODE

///<<<rpc begin
void GsServiceImpl::EnterGs(::google::protobuf::RpcController* controller,
    const gsservice::EnterGsRequest* request,
    gsservice::EnterGsRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterGs
	//第一次进入世界,但是gate还没进入
	auto it = g_players.emplace(request->player_id(), EntityPtr());
	auto player = it.first->second.entity();
	reg.emplace_or_replace<GateConnId>(player, request->conn_id());
	reg.emplace_or_replace<Guid>(player, request->player_id());
	auto msit = g_ms_nodes.find(request->ms_node_id());
	if (msit != g_ms_nodes.end())
	{
		reg.emplace_or_replace<MsNodeWPtr>(player, msit->second);
	}
	auto gate_it = g_gate_nodes.find(request->gate_node_id());
	if (gate_it == g_gate_nodes.end())
	{
		LOG_ERROR << " gate not found" << request->gate_node_id();
		return;
	}
	auto p_gate = reg.try_get<GateNodePtr>(gate_it->second);
	if (nullptr == p_gate)
	{
		LOG_ERROR << " gate not found" << request->gate_node_id();
		return;
	}
	reg.emplace_or_replace<GateNodeWPtr>(player, *p_gate);
	reg.emplace_or_replace<playercomp::NormalLogin>(player);
///<<< END WRITING YOUR CODE EnterGs
}

void GsServiceImpl::PlayerService(::google::protobuf::RpcController* controller,
    const gsservice::MsPlayerMessageRequest* request,
    gsservice::MsPlayerMessageRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerService
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
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(g_serviceinfo[msg_id].method);
	if (nullptr == method)
	{
		LOG_INFO << "msg not found " << msg_id;
		//todo client error;
		return;
	}
	std::unique_ptr<google::protobuf::Message> player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(player_msg.body());
	std::unique_ptr<google::protobuf::Message> player_response(service->GetResponsePrototype(method).New());
	serviceimpl->CallMethod(method, it->second, get_pointer(player_request), get_pointer(player_response));
	if (nullptr == response)//不需要回复
	{
		return;
	}
	response->mutable_ex()->set_player_id(request->ex().player_id());
	response->mutable_msg()->set_body(player_response->SerializeAsString());
	response->mutable_msg()->set_msg_id(msg_id);
///<<< END WRITING YOUR CODE PlayerService
}

void GsServiceImpl::PlayerServiceNoRespone(::google::protobuf::RpcController* controller,
    const gsservice::MsPlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerServiceNoRespone
///<<< END WRITING YOUR CODE PlayerServiceNoRespone
}

void GsServiceImpl::GwPlayerService(::google::protobuf::RpcController* controller,
    const gsservice::RpcClientRequest* request,
    gsservice::RpcClientResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE GwPlayerService
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
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(serviceinfo.method);
	if (nullptr == method)
	{
		return;
	}
	auto player = g_players.find(request->player_id());
	if (player == g_players.end())
	{
		return;
	}
	std::unique_ptr<google::protobuf::Message> player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(request->request());
	std::unique_ptr<google::protobuf::Message> player_response(service->GetResponsePrototype(method).New());
	it->second->CallMethod(method, player->second, get_pointer(player_request), get_pointer(player_response));
	response->set_response(player_response->SerializeAsString());
///<<< END WRITING YOUR CODE GwPlayerService
}

void GsServiceImpl::Disconnect(::google::protobuf::RpcController* controller,
    const gsservice::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE Disconnect
 	g_players.erase(request->guid());//todo  应该是ms 通知过来
///<<< END WRITING YOUR CODE Disconnect
}

void GsServiceImpl::GwConnectGs(::google::protobuf::RpcController* controller,
    const gsservice::ConnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE GwConnectGs
	InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
	for (auto e : reg.view<RpcServerConnection>())
	{
		auto& conn = reg.get<RpcServerConnection>(e).conn_;
		if (conn->peerAddress().toIpPort() != rpc_client_peer_addr.toIpPort())
		{
			continue;
		}
		auto& gate_node = *reg.emplace<GateNodePtr>(e, std::make_shared<GateNode>(conn));
		gate_node.node_info_.set_node_id(request->gate_node_id());
		gate_node.node_info_.set_node_type(kGateWayNode);
		g_gate_nodes.emplace(request->gate_node_id(), e);
		LOG_INFO << "gate node id " << request->gate_node_id();
		break;
	}
///<<< END WRITING YOUR CODE GwConnectGs
}

void GsServiceImpl::RgEnterRoom(::google::protobuf::RpcController* controller,
    const gsservice::Rg2gTest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE RgEnterRoom
///<<< END WRITING YOUR CODE RgEnterRoom
}

void GsServiceImpl::CoverPlayer(::google::protobuf::RpcController* controller,
    const gsservice::CoverPlayerRequest* request,
    gsservice::CoverPlayerRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE CoverPlayer
	auto it = g_players.emplace(request->player_id(), EntityPtr());
	auto player = it.first->second.entity();
	reg.emplace_or_replace<GateConnId>(player, request->conn_id());
	reg.emplace_or_replace<Guid>(player, request->player_id());
	auto msit = g_ms_nodes.find(request->ms_node_id());
	if (msit != g_ms_nodes.end())
	{
		reg.emplace_or_replace<MsNodeWPtr>(player, msit->second);
	}
	;
	auto gate_it = g_gate_nodes.find(request->gate_node_id());
	if (gate_it == g_gate_nodes.end())
	{
		LOG_ERROR << " gate not found" << request->gate_node_id();
		return;
	}
	auto p_gate = reg.try_get<GateNodePtr>(gate_it->second);
	if (nullptr == p_gate)
	{
		LOG_ERROR << " gate not found" << request->gate_node_id();
		return;
	}
	reg.emplace_or_replace<GateNodeWPtr>(player, *p_gate);
	reg.emplace_or_replace<playercomp::CoverPlayerLogin>(player);
///<<< END WRITING YOUR CODE CoverPlayer
}

///<<<rpc end
