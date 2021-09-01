#include "service_l2ms.h"

#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/factories/scene_factories.hpp"
#include "src/game_logic/comp/player.hpp"
#include "src/game_logic/game_registry.h"
#include "src/master_player/master_player_list.h"
#include "src/master_server.h"
#include "src/server_common/closure_auto_done.h"
#include "src/sys/scene_sys.hpp"

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

        ms2gw::PlayerEnterGameServerRequest gw_request;
        gw_request.set_connection_id(connection_id);

        GetWeightRoundRobinSceneParam weight_round_robin_scene;
        weight_round_robin_scene.scene_config_id_ = 0;// has not scene
        auto scene_entity = GetWeightRoundRobinMainScene(reg(), weight_round_robin_scene);
        response->set_game_server_id(reg().get<GameServerDataPtr>(scene_entity)->server_id());
    }

    void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller, 
        const ::l2ms::LeaveGameRequest* request, 
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto player_id = request->player_id();
        auto e = MasterPlayerList::GetSingleton().GetPlayer(player_id);
        assert(reg().get<GameGuid>(e) == player_id);
        reg().destroy(e);
        MasterPlayerList::GetSingleton().LeaveGame(player_id);  
        assert(!MasterPlayerList::GetSingleton().HasPlayer(player_id));
        assert(MasterPlayerList::GetSingleton().GetPlayer(player_id) == entt::null); 
    }

    void LoginServiceImpl::Disconect(::google::protobuf::RpcController* controller, 
        const ::l2ms::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto player_id = request->player_id();
        auto e = MasterPlayerList::GetSingleton().GetPlayer(player_id);
        if (entt::null  == e)
        {
            return;
        }
        assert(reg().get<GameGuid>(e) == player_id);
        reg().destroy(e);
        MasterPlayerList::GetSingleton().LeaveGame(player_id);
        assert(!MasterPlayerList::GetSingleton().HasPlayer(player_id));
        assert(MasterPlayerList::GetSingleton().GetPlayer(player_id) == entt::null);
    }

}//namespace master



