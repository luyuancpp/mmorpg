#include "service.h"

#include "src/common_type/common_type.h"
#include "src/server_registry/server_registry.h"
#include "src/rpc_closure_param/closure_auto_done.h"
#include "src/master_player/master_player_list.h"

namespace l2ms
{
    void l2ms::LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
        const ::l2ms::EnterGameRequest* request,
        ::l2ms::EnterGameResponse* response,
        ::google::protobuf::Closure* done)
    {
        auto player_id = request->player_id();
        common::ClosurePtr cp(done);
        auto e = common::reg().create();
        common::reg().emplace<common::GameGuid>(e, player_id);
        common::reg().emplace<std::string>(e, request->account());
        master::MasterPlayerList::GetSingleton().EnterGame(player_id, e);
    }

    void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller, 
        const ::l2ms::LeaveGameRequest* request, 
        ::l2ms::LeaveGameResponse* response,
        ::google::protobuf::Closure* done)
    {
        auto player_id = request->player_id();
        auto e = master::MasterPlayerList::GetSingleton().GetPlayer(player_id);
        if (e != entt::null)
        {
            assert(common::reg().get<common::GameGuid>(e) == player_id);
            common::reg().destroy(e);
        }
        master::MasterPlayerList::GetSingleton().LeaveGame(player_id);
        assert(master::MasterPlayerList::GetSingleton().HasPlayer(player_id));
        assert(master::MasterPlayerList::GetSingleton().GetPlayer(player_id) == entt::null);
        common::ClosurePtr cp(done);
    }

    void LoginServiceImpl::Disconect(::google::protobuf::RpcController* controller, 
        const ::l2ms::DisconectRequest* request,
        ::l2ms::DisconectResponse* response,
        ::google::protobuf::Closure* done)
    {
        common::ClosurePtr cp(done);
    }

}//namespace master



