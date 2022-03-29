#include "gw2ms.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/net/InetAddress.h"

#include "src/network/gate_node.h"
#include "src/network/gs_node.h"
#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/master_server.h"
#include "src/master_player/ms_player_list.h"
#include "src/scene/sceces.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/server_component.h"
#include "src/sys/servernode_sys.hpp"

using namespace common;
using namespace master;
///<<< END WRITING YOUR CODE

using namespace common;
namespace gw2ms{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void Gw2msServiceImpl::GwConnectMaster(::google::protobuf::RpcController* controller,
    const gw2ms::ConnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE GwConnectMaster
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
///<<< END WRITING YOUR CODE GwConnectMaster
}

void Gw2msServiceImpl::PlayerDisconnect(::google::protobuf::RpcController* controller,
    const gw2ms::PlayerDisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerDisconnect
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
///<<< END WRITING YOUR CODE PlayerDisconnect
}

void Gw2msServiceImpl::LeaveGame(::google::protobuf::RpcController* controller,
    const gw2ms::LeaveGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE LeaveGame
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
///<<< END WRITING YOUR CODE LeaveGame
}

void Gw2msServiceImpl::PlayerService(::google::protobuf::RpcController* controller,
    const gw2ms::ClientMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerService
///<<< END WRITING YOUR CODE PlayerService
}

void Gw2msServiceImpl::Disconnect(::google::protobuf::RpcController* controller,
    const gw2ms::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE Disconnect
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
///<<< END WRITING YOUR CODE Disconnect
}

 ///<<<rpc end
}// namespace gw2ms
