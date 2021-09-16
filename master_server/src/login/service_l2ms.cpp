#include "service_l2ms.h"

#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/factories/scene_factories.hpp"
#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/comp/player.hpp"
#include "src/game_logic/game_registry.h"
#include "src/master_player/master_player_list.h"
#include "src/master_server.h"
#include "src/server_common/closure_auto_done.h"
#include "src/sys/scene_sys.hpp"
#include "src/sys/message_sys.hpp"

#include "ms2g.pb.h"
#include "ms2gw.pb.h"

using namespace master;
using namespace common;

namespace l2ms
{
    void l2ms::LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
        const ::l2ms::EnterGameRequest* request,
        ::l2ms::EnterGameResponese* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto player_id = request->player_id();   
        auto connection_id = request->connection_id();
        auto e = reg().create();
        reg().emplace<GameGuid>(e, player_id);
        reg().emplace<SharedAccountString>(e, std::make_shared<std::string>(request->account()));
        reg().emplace<GatewayConnectionId>(e, connection_id);
        MasterPlayerList::GetSingleton().EnterGame(player_id, e);
        reg().get<common::ConnectionPlayerEnitiesMap>(global_entity()).emplace(connection_id, e);

        ms2gw::PlayerEnterGameServerRequest gw_request;
        gw_request.set_connection_id(connection_id);

        GetWeightRoundRobinSceneParam weight_round_robin_scene;
        weight_round_robin_scene.scene_config_id_ = 1;// has not scene
        auto scene_entity = GetWeightRoundRobinMainScene(reg(), weight_round_robin_scene);
        auto& server_ptr = reg().get<GameServerDataPtr>(scene_entity);
        response->set_game_node_id(server_ptr->node_id());

        auto& gs = reg().get<RpcServerConnection>(server_ptr->server_entity());

        reg().emplace_or_replace<GameServerEntity>(e, GameServerEntity{ server_ptr->server_entity() });

        ::ms2g::EnterGameRequest entergame_request;
        entergame_request.set_player_id(player_id);
        entergame_request.mutable_scenes_info()->set_scene_config_id(reg().get<SceneConfig>(scene_entity).scene_config_id());
        entergame_request.mutable_scenes_info()->set_scene_id(reg().get<GameGuid>(scene_entity));        
        Send2GameServer(entergame_request, "ms2g.Ms2gService", "EnterGame", e);

        EnterSceneParam enter_scene_param;
        enter_scene_param.enter_entity_ = e;
        enter_scene_param.scene_entity_ = scene_entity;
        EnterScene(reg(), enter_scene_param);
    }

    void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller, 
        const ::l2ms::LeaveGameRequest* request, 
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto& connection_map = reg().get<common::ConnectionPlayerEnitiesMap>(global_entity());
        auto it = connection_map.find(request->connection_id());
        assert(it != connection_map.end());
        if (it == connection_map.end())
        {
            return;
        }
        auto player_entity = it->second;

        LeaveSceneParam leave_scene;
        leave_scene.leave_entity_ = player_entity;
        LeaveScene(reg(), leave_scene);

        auto player_id = reg().get<GameGuid>(player_entity);
        assert(MasterPlayerList::GetSingleton().HasPlayer(player_id));  
        reg().destroy(player_entity);
        MasterPlayerList::GetSingleton().LeaveGame(player_id);  
        assert(!MasterPlayerList::GetSingleton().HasPlayer(player_id));

        connection_map.erase(it);
    }

    void LoginServiceImpl::Disconect(::google::protobuf::RpcController* controller, 
        const ::l2ms::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
       auto& connection_map = reg().get<common::ConnectionPlayerEnitiesMap>(global_entity());
        auto it = connection_map.find(request->connection_id());
        if (it == connection_map.end())
        {
            return;
        }
        auto player_entity = it->second;
        auto player_id = reg().get<GameGuid>(player_entity);

        reg().destroy(player_entity);
        connection_map.erase(it);

        MasterPlayerList::GetSingleton().LeaveGame(player_id);
        assert(!MasterPlayerList::GetSingleton().HasPlayer(player_id));
    }

}//namespace master



