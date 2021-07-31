#include "service_l2ms.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/player.hpp"
#include "src/game_logic/game_registry.h"
#include "src/server_common/closure_auto_done.h"
#include "src/master_player/master_player_list.h"

using namespace master;

namespace l2ms
{
    void l2ms::LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
        const ::l2ms::EnterGameRequest* request,
        ::l2ms::EnterGameResponse* response,
        ::google::protobuf::Closure* done)
    {
        common::ClosurePtr cp(done);
        auto player_id = request->player_id();        
        auto e = common::reg().create();
        common::reg().emplace<common::GameGuid>(e, player_id);
        common::reg().emplace<common::SharedAccountString>(e, std::make_shared<std::string>(request->account()));
        MasterPlayerList::GetSingleton().EnterGame(player_id, e);
    }

    void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller, 
        const ::l2ms::LeaveGameRequest* request, 
        ::l2ms::LeaveGameResponse* response,
        ::google::protobuf::Closure* done)
    {
        common::ClosurePtr cp(done);
        auto player_id = request->player_id();
        auto e = MasterPlayerList::GetSingleton().GetPlayer(player_id);
        if (e != entt::null)
        {
            assert(common::reg().get<common::GameGuid>(e) == player_id);
            common::reg().destroy(e);
        }
        MasterPlayerList::GetSingleton().LeaveGame(player_id);
        assert(!MasterPlayerList::GetSingleton().HasPlayer(player_id));
        assert(MasterPlayerList::GetSingleton().GetPlayer(player_id) == entt::null);        
    }

    void LoginServiceImpl::Disconect(::google::protobuf::RpcController* controller, 
        const ::l2ms::DisconectRequest* request,
        ::l2ms::DisconectResponse* response,
        ::google::protobuf::Closure* done)
    {
        common::ClosurePtr cp(done);
    }

}//namespace master



