#include "ms_node.h"
#include "src/server_common/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
/// #include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

#include "src/game_config/mainscene_config.h"

#include "src/network/gate_node.h"
#include "src/network/gs_node.h"
#include "src/factories/scene_factories.hpp"
#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/master_server.h"
#include "src/master_player/ms_player_list.h"
#include "src/scene/sceces.h"
#include "src/server_common/node_info.h"
#include "src/server_common/server_component.h"
#include "src/sys/servernode_sys.hpp"

using namespace master;

using Ms2GsStubPtr = std::unique_ptr <common::RpcStub<ms2gs::Ms2gService_Stub>>;

std::size_t kMaxPlayerSize = 1000;
///<<< END WRITING YOUR CODE

using namespace common;
namespace msservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void MasterNodeServiceImpl::StartGS(::google::protobuf::RpcController* controller,
    const msservice::StartGSRequest* request,
    msservice::StartGSResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartGS
	response->set_master_node_id(g_ms_node->master_node_id());
	InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
	InetAddress rpc_server_peer_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity gs_entity{ entt::null };
	for (auto e : reg.view<RpcServerConnection>())
	{
		if (reg.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != rpc_client_peer_addr.toIpPort())
		{
			continue;
		}
		gs_entity = e;
		break;
	}
	if (gs_entity == entt::null)
	{
		//todo
		LOG_INFO << "game connection not found " << request->gs_node_id();
		return;
	}

	auto c = reg.get<RpcServerConnection>(gs_entity);
	GsNodePtr gs = std::make_shared<GsNode>(c.conn_);
	gs->node_info_.set_node_id(request->gs_node_id());
	gs->node_info_.set_node_type(GAME_SERVER_NODE_TYPE);
	MakeGSParam make_gs_p;
	make_gs_p.node_id_ = request->gs_node_id();
	AddMainSceneNodeCompnent(gs_entity, make_gs_p);
	reg.emplace<InetAddress>(gs_entity, rpc_server_peer_addr);
	reg.emplace<GsNodePtr>(gs_entity, gs);

	reg.emplace<Ms2GsStubPtr>(gs_entity, std::make_unique<Ms2GsStubPtr::element_type>(boost::any_cast<muduo::net::RpcChannelPtr>(c.conn_->getContext())));
	if (request->server_type() == kMainServer)
	{
		auto& config_all = mainscene_config::GetSingleton().all();
		MakeGSSceneP create_scene_param;
		create_scene_param.server_entity_ = gs_entity;
		for (int32_t i = 0; i < config_all.data_size(); ++i)
		{
			create_scene_param.scene_confid_ = config_all.data(i).id();
			auto scene_entity = g_scene_sys->MakeSceneGSScene(create_scene_param);
			if (!reg.valid(scene_entity))
			{
				continue;
			}
			auto scene_info = response->add_scenes_info();
			scene_info->set_scene_confid(reg.get<ConfigIdComp>(scene_entity));
			scene_info->set_scene_id(reg.get<Guid>(scene_entity));
		}
	}
	else
	{
		reg.remove<MainSceneServerComp>(gs_entity);
		reg.emplace<RoomSceneServerComp>(gs_entity);
	}

	for (auto e : reg.view<GateNodePtr>())
	{
		g_ms_node->DoGateConnectGs(gs_entity, e);
	}
	g_ms_node->OnGsNodeStart(gs_entity);
	LOG_INFO << "game connected " << request->gs_node_id();
///<<< END WRITING YOUR CODE StartGS
}

void MasterNodeServiceImpl::OnGwConnect(::google::protobuf::RpcController* controller,
    const msservice::ConnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE OnGwConnect
	InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
	entt::entity gate_entity{ entt::null };
	for (auto e : reg.view<RpcServerConnection>())
	{
		auto c = reg.get<RpcServerConnection>(e);
		auto& local_addr = c.conn_->peerAddress();
		if (local_addr.toIpPort() != rpc_client_peer_addr.toIpPort())
		{
			continue;
		}
		gate_entity = e;
		auto& gate_nodes = reg.get<GateNodes>(global_entity());
		auto& gate_node = *reg.emplace<GateNodePtr>(gate_entity, std::make_shared<GateNode>(c.conn_));
		gate_node.node_info_.set_node_id(request->gate_node_id());
		gate_node.node_info_.set_node_type(GATEWAY_NODE_TYPE);
		gate_nodes.emplace(request->gate_node_id(), gate_entity);
		break;
	}

	for (auto e : reg.view<GsNodePtr>())
	{
		g_ms_node->DoGateConnectGs(e, gate_entity);
	}
///<<< END WRITING YOUR CODE OnGwConnect
}

void MasterNodeServiceImpl::OnGwPlayerDisconnect(::google::protobuf::RpcController* controller,
    const msservice::PlayerDisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE OnGwPlayerDisconnect
	auto& connection_map = reg.get<ConnectionPlayerEnitiesMap>(global_entity());
	auto it = connection_map.find(request->conn_id());
	if (it == connection_map.end())
	{
		return;
	}
	auto player_entity = it->second;
	auto guid = reg.get<Guid>(player_entity);

	reg.destroy(player_entity);
	connection_map.erase(it);

	PlayerList::GetSingleton().LeaveGame(guid);
	assert(!PlayerList::GetSingleton().HasPlayer(guid));
///<<< END WRITING YOUR CODE OnGwPlayerDisconnect
}

void MasterNodeServiceImpl::OnGwLeaveGame(::google::protobuf::RpcController* controller,
    const msservice::LeaveGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE OnGwLeaveGame

	auto& connection_map = reg.get<ConnectionPlayerEnitiesMap>(global_entity());
	auto it = connection_map.find(request->conn_id());
	assert(it != connection_map.end());
	if (it == connection_map.end())
	{
		return;
	}
	auto player_entity = it->second;

	LeaveSceneParam leave_scene;
	leave_scene.leave_entity_ = player_entity;
	g_scene_sys->LeaveScene(leave_scene);

	auto guid = reg.get<Guid>(player_entity);
	assert(PlayerList::GetSingleton().HasPlayer(guid));
	reg.destroy(player_entity);
	PlayerList::GetSingleton().LeaveGame(guid);
	assert(!PlayerList::GetSingleton().HasPlayer(guid));

	connection_map.erase(it);
///<<< END WRITING YOUR CODE OnGwLeaveGame
}

void MasterNodeServiceImpl::OnGwPlayerService(::google::protobuf::RpcController* controller,
    const msservice::ClientMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE OnGwPlayerService
///<<< END WRITING YOUR CODE OnGwPlayerService
}

void MasterNodeServiceImpl::OnGwDisconnect(::google::protobuf::RpcController* controller,
    const msservice::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE OnGwDisconnect
	auto guid = request->guid();
	auto e = PlayerList::GetSingleton().GetPlayer(guid);
	if (entt::null == e)
	{
		return;
	}
	assert(reg.get<Guid>(e) == guid);
	reg.destroy(e);
	PlayerList::GetSingleton().LeaveGame(guid);
	assert(!PlayerList::GetSingleton().HasPlayer(guid));
	assert(PlayerList::GetSingleton().GetPlayer(guid) == entt::null);
///<<< END WRITING YOUR CODE OnGwDisconnect
}

///<<<rpc end
}// namespace msservice
