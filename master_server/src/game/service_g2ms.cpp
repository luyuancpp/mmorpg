#include "service_g2ms.h"

#include "muduo/net/InetAddress.h"

#include "src/game/game_client.h"
#include "src/game_logic/game_registry.h"
#include "src/master_server.h"
#include "src/server_common/server_component.h"

using namespace master;
using namespace common;
using namespace muduo::net;

namespace g2ms
{
    void G2msServiceImpl::StartLogicServer(::google::protobuf::RpcController* controller, 
        const ::g2ms::StartLogicServerRequest* request, 
        ::google::protobuf::Empty* response, 
        ::google::protobuf::Closure* done)
    {
        InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
        InetAddress rpc_server_peer_addr(request->rpc_server().ip(), request->rpc_server().port());

        for (auto e : reg().view<RpcServerConnection>())
        {
            auto c = reg().get<RpcServerConnection>(e);
            auto& local_addr = c.conn_->peerAddress();
            if (local_addr.toIp() != rpc_client_peer_addr.toIp() ||
                local_addr.port() != rpc_client_peer_addr.port())
            {
                continue;
            }
            auto ce = GameClient::GetSingleton()->create();
            GameClient::GetSingleton()->emplace<common::RpcServerConnection>(ce, common::RpcServerConnection{ c.conn_ });
            GameClient::GetSingleton()->emplace<InetAddress>(ce, rpc_server_peer_addr);

            if (nullptr == g_master_server->gate_client() || !g_master_server->gate_client()->Connected())
            {
                common::WaitingGatewayConnecting wgc{ rpc_server_peer_addr };
                auto e = reg().create();
                reg().emplace<common::WaitingGatewayConnecting>(e, wgc);
            }
            else
            {
                g_master_server->GatewayConnectGame(rpc_server_peer_addr);
            }
            break;
        }

    }

}//namespace g2ms
