#include "service_l2ms.h"

#include "muduo/base/Logging.h"

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
    void LoginServiceImpl::LoginAccount(::google::protobuf::RpcController* controller,
        const ::l2ms::LoginAccountRequest* request,
        ::l2ms::LoginAccountResponse* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        auto lit = login_accounts_.find(request->account());
        if (lit == login_accounts_.end() && 
            (MasterPlayerList::GetSingleton().player_size() + login_accounts_.size()) >= kMaxPlayerSize)
        {
            //如果登录的是新账号,满了得去排队
            response->mutable_error()->set_error_no(RET_LOGIN_MAX_PLAYER_SIZE);
            return;
        }
        
        if (lit != login_accounts_.end())
        {
            auto& lc = lit->second;
            //如果不是同一个登录服务器,踢掉已经登录的账号
            if (reg().get<AccountLoginNode>(lc.entity()).node_id_ != request->node_id())
            {

            }
            else//告诉客户端登录中
            {
                response->mutable_error()->set_error_no(RET_LOGIN_LOGIN_ING);
            }
        }
        else
        {
            auto result = login_accounts_.emplace(request->account(), MSLoginAccount());
            if (result.second)
            {
                auto& lc = result.first->second;
                reg().emplace<SharedAccountString>(lc.entity(), std::make_shared<std::string>(request->account()));
                reg().emplace<AccountLoginNode>(lc.entity(), AccountLoginNode{ request->node_id()});
            }
        }
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



