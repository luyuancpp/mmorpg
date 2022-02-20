#include "l2ms.h"
///<<< BEGIN WRITING YOUR CODE
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
///<<< END WRITING YOUR CODE

namespace l2ms
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
///<<<rpc begin
    void LoginServiceImpl::LoginAccount(::google::protobuf::RpcController* controller,
        const ::l2ms::LoginAccountRequest* request,
        ::l2ms::LoginAccountResponse* response,
        ::google::protobuf::Closure* done)
    {
///<<< BEGIN WRITING YOUR CODE
        AutoRecycleClosure cp(done);
        auto lit = logined_accounts_.find(request->account());
        if (lit == logined_accounts_.end() && 
            (PlayerList::GetSingleton().player_size() + logined_accounts_.size()) >= kMaxPlayerSize)
        {
            //如果登录的是新账号,满了得去排队
            response->mutable_error()->set_error_no(RET_LOGIN_MAX_PLAYER_SIZE);
            return;
        }
        
        if (lit != logined_accounts_.end())
        {
            auto& lc = lit->second;
            //如果不是同一个登录服务器,踢掉已经登录的账号
            if (reg.get<AccountLoginNode>(lc.entity()).node_id_ != request->login_node_id())
            {

            }
            else//告诉客户端登录中
            {
                response->mutable_error()->set_error_no(RET_LOGIN_LOGIN_ING);
            }
        }
        else
        {
            auto result = logined_accounts_.emplace(request->account(), MSLoginAccount());
            if (result.second)
            {
                auto& lc = result.first->second;
                reg.emplace<SharedAccountString>(lc.entity(), std::make_shared<std::string>(request->account()));
                reg.emplace<AccountLoginNode>(lc.entity(), AccountLoginNode{ request->login_node_id()});
            }
        }
 ///<<< END WRITING YOUR CODE
    }

    void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
        const ::l2ms::EnterGameRequest* request,
        ::l2ms::EnterGameResponese* response,
        ::google::protobuf::Closure* done)
    {
///<<< BEGIN WRITING YOUR CODE
        AutoRecycleClosure cp(done);
        auto guid = request->guid();   
        auto connection_id = request->connection_id();
        auto e = reg.create();
        reg.emplace<Guid>(e, guid);
        reg.emplace<GatewayConnectionId>(e, connection_id);
        PlayerList::GetSingleton().EnterGame(guid, e);
        ms2gw::PlayerEnterGSRequest gw_request;
        gw_request.set_connection_id(connection_id);//error
        for (auto e : SessionReg::GetSingleton().view<uint32_t>())
        {
            response->set_gs_node_id(SessionReg::GetSingleton().get<uint32_t>(e));
            break;
        }
 ///<<< END WRITING YOUR CODE
    }

    void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller, 
        const ::l2ms::LeaveGameRequest* request, 
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
///<<< BEGIN WRITING YOUR CODE
        AutoRecycleClosure cp(done);
        auto guid = request->guid();
        auto e = PlayerList::GetSingleton().GetPlayer(guid);
        assert(reg.get<Guid>(e) == guid);
        reg.destroy(e);
        PlayerList::GetSingleton().LeaveGame(guid);  
        assert(!PlayerList::GetSingleton().HasPlayer(guid));
        assert(PlayerList::GetSingleton().GetPlayer(guid) == entt::null); 
    }

    void LoginServiceImpl::Disconect(::google::protobuf::RpcController* controller, 
        const ::l2ms::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
///<<< BEGIN WRITING YOUR CODE
        AutoRecycleClosure cp(done);
        auto guid = request->guid();
        auto e = PlayerList::GetSingleton().GetPlayer(guid);
        if (entt::null  == e)
        {
            return;
        }
        assert(reg.get<Guid>(e) == guid);
        reg.destroy(e);
        PlayerList::GetSingleton().LeaveGame(guid);
        assert(!PlayerList::GetSingleton().HasPlayer(guid));
        assert(PlayerList::GetSingleton().GetPlayer(guid) == entt::null);
 ///<<< END WRITING YOUR CODE
    }
///<<<rpc end
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}//namespace master



