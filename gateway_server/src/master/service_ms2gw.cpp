#include "service_ms2gw.h"

#include "src/game/game_client.h"
#include "src/game_logic/comp/player.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/entity_cast.h"
#include "src/gate_player/gate_player_list.h"
#include "src/gateway_server.h"
#include "src/server_common/closure_auto_done.h"

#include "gw2g.pb.h"

using namespace  gateway;
using namespace  common;

namespace ms2gw
{
    void Ms2gwServiceImpl::StartGameServer(::google::protobuf::RpcController* controller,
        const ::ms2gw::StartGameServerRequest* request, 
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        InetAddress gs_addr(request->ip(), request->port());
        for (auto e : reg().view<InetAddress>())
        {
            auto& c = reg().get<InetAddress>(e);
            if (gs_addr.toIpPort() == c.toIpPort())
            {
                return;
            }
        }
        auto e = GameClient::GetSingleton().create();
        auto& c = GameClient::GetSingleton().emplace<RpcClientPtr>(e, 
            std::make_unique<RpcClient>(EventLoop::getEventLoopOfCurrentThread(), gs_addr));
        using Gw2gStubPtr = RpcStub<gw2g::Gw2gService_Stub>::MyType;
        auto& sc =  GameClient::GetSingleton().emplace<Gw2gStubPtr>(e, std::make_unique<RpcStub<gw2g::Gw2gService_Stub>>());
        c->subscribe<RegisterStubES>(*(sc.get()));
        c->connect();
        GameClient::GetSingleton().emplace<InetAddress>(e, gs_addr);
        GameClient::GetSingleton().emplace<uint32_t>(e, request->node_id());
        LOG_INFO << "connect to game server " << gs_addr.toIpPort() << " server id " << request->node_id();
    }

    void Ms2gwServiceImpl::StopGameServer(::google::protobuf::RpcController* controller, 
        const ::ms2gw::StopGameServerRequest* request, 
        ::google::protobuf::Empty* response, 
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        for (auto e : GameClient::GetSingleton().view<InetAddress>())
        {
            auto& c = GameClient::GetSingleton().get<InetAddress>(e);
            if (c.toIp() != request->ip() ||
                c.port() != request->port())
            {
                continue;
            }
            GameClient::GetSingleton().destroy(e);
            break;
        }
    }

    void Ms2gwServiceImpl::PlayerEnterGameServer(::google::protobuf::RpcController* controller, 
        const ::ms2gw::PlayerEnterGameServerRequest* request, 
        ::google::protobuf::Empty* response, 
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto it = g_gate_clients_->find(request->connection_id());
        if (it == g_gate_clients_->end())
        {
            return;
        }
        it->second.node_id_ = request->node_id();
    }
}


