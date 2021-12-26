#include "service_l2ms.h"

#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/player_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game/game_client.h"
#include "src/master_player/ms_player_list.h"
#include "src/master_server.h"
#include "src/return_code/error_code.h"
#include "src/server_common/closure_auto_done.h"

#include "ms2gw.pb.h"

using namespace master;
using namespace common;

std::size_t kMaxPlayerSize = 1000;

namespace l2ms
{

    void LoginServiceImpl::Login(::google::protobuf::RpcController* controller,
        const ::l2ms::LoginRequest* request, 
        ::l2ms::LoginResponse* response, 
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        if ((MasterPlayerList::GetSingleton().player_size() + login_accounts_.size()) >= kMaxPlayerSize)
        {
            response->mutable_error()->set_error_no(RET_LOGIN_MAX_PLAYER_SIZE);
            return;
        }
        if (login_accounts_.find(request->account()) != login_accounts_.end())
        {
            return;
        }
        auto result = login_accounts_.emplace(request->account(), LoginAccount());
        if (result.second)
        {
            auto& lc = result.first->second;
            reg().emplace<SharedAccountString>(lc.entity(), std::make_shared<std::string>(request->account()));
            lc.login_state_machine_.Login();
        }
        response->mutable_error()->set_error_no(RET_OK);
    }

    void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
        const ::l2ms::EnterGameRequest* request,
        ::l2ms::EnterGameResponese* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto guid = request->guid();   
        auto connection_id = request->connection_id();
        auto e = reg().create();
        reg().emplace<Guid>(e, guid);
        reg().emplace<GatewayConnectionId>(e, connection_id);
        MasterPlayerList::GetSingleton().EnterGame(guid, e);
        ms2gw::PlayerEnterGSRequest gw_request;
        gw_request.set_connection_id(connection_id);//error
        for (auto e : GameClient::GetSingleton().view<uint32_t>())
        {
            response->set_node_id(GameClient::GetSingleton().get<uint32_t>(e));
            break;
        }
    }

    void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller, 
        const ::l2ms::LeaveGameRequest* request, 
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto guid = request->guid();
        auto e = MasterPlayerList::GetSingleton().GetPlayer(guid);
        assert(reg().get<Guid>(e) == guid);
        reg().destroy(e);
        MasterPlayerList::GetSingleton().LeaveGame(guid);  
        assert(!MasterPlayerList::GetSingleton().HasPlayer(guid));
        assert(MasterPlayerList::GetSingleton().GetPlayer(guid) == entt::null); 
    }

    void LoginServiceImpl::Disconect(::google::protobuf::RpcController* controller, 
        const ::l2ms::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto guid = request->guid();
        auto e = MasterPlayerList::GetSingleton().GetPlayer(guid);
        if (entt::null  == e)
        {
            return;
        }
        assert(reg().get<Guid>(e) == guid);
        reg().destroy(e);
        MasterPlayerList::GetSingleton().LeaveGame(guid);
        assert(!MasterPlayerList::GetSingleton().HasPlayer(guid));
        assert(MasterPlayerList::GetSingleton().GetPlayer(guid) == entt::null);
    }

}//namespace master



