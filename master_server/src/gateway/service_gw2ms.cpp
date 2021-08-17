#include "service_gw2ms.h"

#include "muduo/net/InetAddress.h"

#include "src/game/game_client.h"
#include "src/game_logic/game_registry.h"
#include "src/master_server.h"
#include "src/server_common/closure_auto_done.h"
#include "src/server_common/server_component.h"

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
            if (local_addr.toIp() != rpc_client_peer_addr.toIp() ||
                local_addr.port() != rpc_client_peer_addr.port())
            {
                continue;
            }
            g_master_server->gate_client() =  std::make_unique<RpcServerConnection>(c.conn_);
            break;
        }
        for (auto e : GameClient::GetSingleton()->view<muduo::net::InetAddress>())
        {
            g_master_server->GatewayConnectGame(e);
        }

    }
}//namespace gw2ms
