#include "gs2ms.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

#include "src/game_config/mainscene_config.h"

#include "src/game/gs_node.h"
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

namespace gs2ms{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void G2msServiceImpl::StartGS(::google::protobuf::RpcController* controller,
    const gs2ms::StartGSRequest* request,
    gs2ms::StartGSResponse* response,
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
        auto c = reg.get<RpcServerConnection>(e);
        auto& local_addr = c.conn_->peerAddress();
        if (local_addr.toIpPort() != rpc_client_peer_addr.toIpPort())
        {
            continue;
        }
        gs_entity = e;
        break;            
    }
    if (gs_entity == entt::null)
    {
		//todo
        LOG_INFO << "game connection not found " << request->node_id();
        return;
    }
   
    auto c = reg.get<RpcServerConnection>(gs_entity);
    GsNodePtr gs = std::make_shared<GsNode>(c.conn_);
	gs->node_info_.node_id_ = request->node_id();
	gs->node_info_.node_type_ = GAME_SERVER_NODTE_TYPE;
    MakeGSParam make_gs_p;
    make_gs_p.node_id_ = request->node_id();
    AddMainSceneNodeCompnent(gs_entity, make_gs_p);
    reg.emplace<InetAddress>(gs_entity, rpc_server_peer_addr);
    reg.emplace<GsNodePtr>(gs_entity, gs);
    if (request->server_type() == kMainServer)
    {
        auto& config_all = mainscene_config::GetSingleton().all();
        MakeGSSceneP create_scene_param;
        create_scene_param.server_entity_ = gs_entity;
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
        reg.remove<MainSceneServerComp>(gs_entity);
        reg.emplace<RoomSceneServerComp>(gs_entity);
    }
    g_ms_node->GatewayConnectGame(gs_entity);
    g_ms_node->OnGsNodeStart(gs_entity);
    LOG_INFO << "game connected " << request->node_id();
///<<< END WRITING YOUR CODE StartGS
}

///<<<rpc end
}// namespace gs2ms
