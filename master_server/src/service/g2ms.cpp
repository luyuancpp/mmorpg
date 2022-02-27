#include "g2ms.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

#include "src/game_config/mainscene_config.h"

#include "src/game_logic/enum/server_enum.h"
#include "src/factories/scene_factories.hpp"
#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/master_server.h"
#include "src/scene/sceces.h"
#include "src/server_common/closure_auto_done.h"
#include "src/server_common/server_component.h"

using namespace master;
using namespace common;
using namespace muduo::net;
///<<< END WRITING YOUR CODE

namespace g2ms{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void G2msServiceImpl::StartGS(::google::protobuf::RpcController* controller,
    const g2ms::StartGSRequest* request,
    g2ms::StartGSResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartGS
    response->set_master_node_id(g_ms_node->master_node_id());
    InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
    InetAddress rpc_server_peer_addr(request->rpc_server().ip(), request->rpc_server().port());

    entt::entity game_server_entity{ entt::null };
    for (auto e : reg.view<RpcServerConnection>())
    {
        auto c = reg.get<RpcServerConnection>(e);
        auto& local_addr = c.conn_->peerAddress();
        if (local_addr.toIpPort() != rpc_client_peer_addr.toIpPort())
        {
            continue;
        }
        game_server_entity = e;
        break;            
    }
    if (game_server_entity == entt::null)
    {
        return;
    }
    auto c = reg.get<RpcServerConnection>(game_server_entity);
    MakeGSParam cparam;
    cparam.node_id_ = request->node_id();
    auto server_entity = MakeMainSceneNode(reg, cparam);
    reg.emplace<RpcServerConnection>(server_entity, RpcServerConnection{ c.conn_ });
    reg.emplace<InetAddress>(server_entity, rpc_server_peer_addr);

    if (request->server_type() == kMainServer)
    {
        auto& config_all = mainscene_config::GetSingleton().all();
        MakeGSSceneP create_scene_param;
        create_scene_param.server_entity_ = server_entity;
        for (int32_t i = 0; i < config_all.data_size(); ++i)
        {
            create_scene_param.scene_confid_ = config_all.data(i).id();
            auto scene_entity = g_scene_sys->MakeSceneGSScene( create_scene_param);
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
        reg.remove<MainSceneServerComp>(server_entity);
        reg.emplace<RoomSceneServerComp>(server_entity);
    }
    g_ms_node->GatewayConnectGame(server_entity);
    LOG_INFO << "game connected " << request->node_id();
///<<< END WRITING YOUR CODE StartGS
}

///<<<rpc end
}// namespace g2ms
