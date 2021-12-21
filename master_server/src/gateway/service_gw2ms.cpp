#include "service_gw2ms.h"

#include "muduo/net/InetAddress.h"

#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/comp/player.hpp"
#include "src/master_server.h"
#include "src/master_player/master_player_list.h"
#include "src/server_common/closure_auto_done.h"
#include "src/server_common/server_component.h"
#include "src/sys/scene_sys.hpp"

using namespace common;
using namespace master;

namespace gw2ms
{
    void Gw2msServiceImpl::GwConnectMaster(::google::protobuf::RpcController* controller, 
        const ::gw2ms::ConnectRequest* request, 
        ::google::protobuf::Empty* response, 
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
        for (auto e : reg().view<RpcServerConnection>())
        {
            auto c = reg().get<RpcServerConnection>(e);
            auto& local_addr = c.conn_->peerAddress();
            if (local_addr.toIpPort() != rpc_client_peer_addr.toIpPort())
            {
                continue;
            }
            g_master_server->gate_client() =  std::make_unique<RpcServerConnection>(c.conn_);
            break;
        }
        for (auto e : reg().view<muduo::net::InetAddress>())
        {
            g_master_server->GatewayConnectGame(e);
        }
    }

    void Gw2msServiceImpl::PlayerDisconnect(::google::protobuf::RpcController* controller, 
        const ::gw2ms::PlayerDisconnectRequest* request, 
        ::google::protobuf::Empty* response, 
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto& connection_map = reg().get<ConnectionPlayerEnitiesMap>(global_entity());
        auto it = connection_map.find(request->connection_id());
        if (it == connection_map.end())
        {
            return;
        }
        auto player_entity = it->second;
        auto guid = reg().get<Guid>(player_entity);

        reg().destroy(player_entity);
        connection_map.erase(it);

        MasterPlayerList::GetSingleton().LeaveGame(guid);
        assert(!MasterPlayerList::GetSingleton().HasPlayer(guid));
    }

    void Gw2msServiceImpl::LeaveGame(::google::protobuf::RpcController* controller, 
        const ::gw2ms::LeaveGameRequest* request, 
        ::google::protobuf::Empty* response, 
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto& connection_map = reg().get<ConnectionPlayerEnitiesMap>(global_entity());
        auto it = connection_map.find(request->connection_id());
        assert(it != connection_map.end());
        if (it == connection_map.end())
        {
            return;
        }
        auto player_entity = it->second;

        LeaveSceneParam leave_scene;
        leave_scene.leave_entity_ = player_entity;
        LeaveScene(leave_scene);

        auto guid = reg().get<Guid>(player_entity);
        assert(MasterPlayerList::GetSingleton().HasPlayer(guid));
        reg().destroy(player_entity);
        MasterPlayerList::GetSingleton().LeaveGame(guid);
        assert(!MasterPlayerList::GetSingleton().HasPlayer(guid));

        connection_map.erase(it);
    }

}//namespace gw2ms
