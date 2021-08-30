#include "service_g2ms.h"

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

#include "src/factories/scene_factories.hpp"
#include "src/game_logic/game_registry.h"
#include "src/master_server.h"
#include "src/server_common/server_component.h"

using namespace master;
using namespace common;
using namespace muduo::net;

namespace g2ms
{
    void G2msServiceImpl::StartGameServer(::google::protobuf::RpcController* controller, 
        const ::g2ms::StartGameServerRequest* request, 
        ::google::protobuf::Empty* response, 
        ::google::protobuf::Closure* done)
    {
        InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
        InetAddress rpc_server_peer_addr(request->rpc_server().ip(), request->rpc_server().port());

        for (auto e : reg().view<RpcServerConnection>())
        {
            auto c = reg().get<RpcServerConnection>(e);
            auto& local_addr = c.conn_->peerAddress();
            if (local_addr.toIpPort() != rpc_client_peer_addr.toIpPort())
            {
                continue;
            }
            MakeGameServerParam cparam;
            cparam.server_id_ = request->server_id();
            auto server_entity = MakeGameServer(reg(), cparam);
            reg().emplace<common::RpcServerConnection>(server_entity, common::RpcServerConnection{ c.conn_ });
            reg().emplace<InetAddress>(server_entity, rpc_server_peer_addr);
            g_master_server->GatewayConnectGame(server_entity);
            LOG_INFO << "game connected " << request->server_id();
            break;
        }

    }

}//namespace g2ms
