#include "service_gw2l.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/server_common/rpc_server.h"
#include "src/server_common/closure_auto_done.h"
#include "src/return_code/error_code.h"
#include "src/login_server.h"

using namespace muduo;
using namespace muduo::net;
using namespace common;

namespace gw2l
{
    LoginServiceImpl::LoginServiceImpl(LoginStubl2ms& l2ms_login_stub,
        LoginStubl2db& l2db_login_stub)
        : l2ms_login_stub_(l2ms_login_stub),
          l2db_login_stub_(l2db_login_stub)
    {}

void LoginServiceImpl::Login(::google::protobuf::RpcController* controller,
    const gw2l::LoginRequest* request,
    gw2l::LoginResponse* response,
    ::google::protobuf::Closure* done)
{ 
    //login master
    LoginMasterRP cp(std::make_shared<LoginMasterRpcString>(response, done));
    auto& s_reqst = cp->s_reqst_;
    s_reqst.set_account(request->account());
    s_reqst.set_login_node_id(g_login_server->node_id());
    s_reqst.set_connection_id(request->connection_id());
    auto it =  connection_accounts_.emplace(request->connection_id(), common::EntityHandle());
    if (it.second)
    {
        reg().emplace<std::string>(it.first->second.entity(), request->account());
    }
    l2ms_login_stub_.CallMethodString(this, &LoginServiceImpl::MSLoginReplied, cp, &l2ms::LoginService_Stub::LoginAccount);
}

void LoginServiceImpl::DbLoginReplied(LoginRP d)
{
    auto& sresp = d->s_resp_;
    d->c_resp_->mutable_account_player()->CopyFrom(sresp->account_player());
    UpdateAccount(d->s_reqst_.account(), sresp->account_player());
}

void LoginServiceImpl::MSLoginReplied(LoginMasterRP d)
{
    //只连接不登录,占用连接
 // login process
 // check account rule: empty , erro
 //check string rule
    auto cit = connection_accounts_.find(d->s_reqst_.connection_id());
    auto& account = d->s_reqst_.account();
    auto& response = d->c_resp_;
    {
        auto it = login_players_.find(account);
        if (it == login_players_.end())
        {
            auto ret = login_players_.emplace(account, std::make_shared<AccountPlayer>());
            it = ret.first;
            reg().emplace<PlayerPtr>(cit->second.entity(), it->second);
        }
        auto& player = it->second;
        auto ret = player->Login();
        if (ret != RET_OK)
        {
            response->mutable_error()->set_error_no(ret);
            return;
        }
        auto& account_data = player->account_data();
        redis_->Load(account_data, account);
        if (!account_data.password().empty())
        {
            response->mutable_account_player()->CopyFrom(account_data);
            player->OnDbLoaded();
            return;
        }
    }

     // database process
    LoginRP cp(std::make_shared<LoginRpcString>(*d));
    auto& s_reqst = cp->s_reqst_;
    s_reqst.set_account(account);
    l2db_login_stub_.CallMethodString(this, &LoginServiceImpl::DbLoginReplied, cp,  &l2db::LoginService_Stub::Login);

}

void LoginServiceImpl::CreatPlayer(::google::protobuf::RpcController* controller,
    const gw2l::CreatePlayerRequest* request, 
    gw2l::CreatePlayerResponse* response, 
    ::google::protobuf::Closure* done)
{
    // login process
    //check name rule
    auto cit = connection_accounts_.find(request->connection_id());
    if (cit == connection_accounts_.end())
    {
        ReturnCloseureError(RET_LOGIN_CREATE_PLAYER_CONNECTION_HAS_NOT_ACCOUNT);
    }
    auto* p_player = reg().try_get<PlayerPtr>(cit->second.entity());
    if (nullptr == p_player)
    {
        ReturnCloseureError(REG_LOGIN_CREATEPLAYER_CONNECTION_ACCOUNT_EMPTY);
    }
    auto& ap = *p_player;
    CheckReturnCloseureError(ap->CreatePlayer());

    // database process
    CreatePlayerRP cp(std::make_shared<CreatePlayerRpcString>(response, done));
    cp->s_reqst_.set_account(ap->account());
    l2db_login_stub_.CallMethodString(this,
        &LoginServiceImpl::DbCreatePlayerReplied,
        cp,
        &l2db::LoginService_Stub::CreatePlayer);
}

void LoginServiceImpl::DbCreatePlayerReplied(CreatePlayerRP d)
{
    auto& sresp = d->s_resp_;
    d->c_resp_->mutable_account_player()->CopyFrom(sresp->account_player());
    UpdateAccount(d->s_reqst_.account(), sresp->account_player());
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ::gw2l::EnterGameRequest* request,
    ::gw2l::EnterGameResponse* response,
    ::google::protobuf::Closure* done)
{
    auto guid = request->guid();
    auto connection_id = request->connection_id();
    auto cit = connection_accounts_.find(connection_id);
    if (cit == connection_accounts_.end())
    {
        ReturnCloseureError(REG_LOGIN_ENTERGAMEE_CONNECTION_ACCOUNT_EMPTY);
    }
    auto* p_player = reg().try_get<PlayerPtr>(cit->second.entity());
    if (nullptr == p_player)
    {
        ReturnCloseureError(REG_LOGIN_CREATEPLAYER_CONNECTION_ACCOUNT_EMPTY);
    }
    // check second times change player id error 
    auto& ap = *p_player;
    CheckReturnCloseureError(ap->EnterGame());

    // long time in login processing
    if (!ap->IsInPlayerList(guid))  
    {
        ReturnCloseureError(RET_LOGIN_ENTER_GUID);
    }
    auto& account = ap->account();
    // player in redis return ok
    player_database new_player;
    redis_->Load(new_player, guid);
    ap->Playing(guid);//test
    response->set_connection_id(connection_id);
    response->set_guid(guid);//test
    if (new_player.guid() > 0)
    {
        CallEnterMS(guid, account, response, done);
        return;
    }        
    // database to redis 
    EnterGameDbRP cp(std::make_shared<EnterGameDbRpcString>(response, done));
    auto& sreqst = cp->s_reqst_;
    sreqst.set_account(account);
    sreqst.set_guid(guid);
    l2db_login_stub_.CallMethodString(this,
        &LoginServiceImpl::EnterGameDbReplied,
        cp,
        &l2db::LoginService_Stub::EnterGame);
}

void LoginServiceImpl::EnterGameDbReplied(EnterGameDbRP d)
{
    auto& sreqst = d->s_reqst_;
    auto cit = login_players_.find(sreqst.account());
    if (cit == login_players_.end())
    {
        LOG_ERROR << "disconnect not found connection id " << d->s_reqst_.account();
        return;
    }
    ::gw2l::EnterGameResponse* response = nullptr;
    ::google::protobuf::Closure* done = nullptr;
    d->Move(response, done);
    CallEnterMS(sreqst.guid(), sreqst.account(), response, done);
}

void LoginServiceImpl::EnterMSReplied(EnterGameMS d)
{
    d->c_resp_->set_node_id(d->s_resp_->node_id());
}

void LoginServiceImpl::CallEnterMS(Guid guid,
    const std::string& account,
    ::gw2l::EnterGameResponse* response,
    ::google::protobuf::Closure* done)
{   
    EnterGameMS cp(std::make_shared<EnterMSRpcString>(response, done));
    cp->s_reqst_.set_account(account);
    cp->s_reqst_.set_guid(guid);
    cp->s_reqst_.set_connection_id(response->connection_id());
    l2ms_login_stub_.CallMethodString(this,
        &LoginServiceImpl::EnterMSReplied,
        cp,
        &l2ms::LoginService_Stub::EnterGame);
}

void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller, 
    const ::gw2l::LeaveGameRequest* request, 
    ::google::protobuf::Empty* response, 
    ::google::protobuf::Closure* done)
{
    ClosurePtr cp(done);
    //连接过，登录过
    auto cit = connection_accounts_.find(request->connection_id());
    if (cit == connection_accounts_.end())
    {
        LOG_ERROR << " leave game not found connection";
        return;
    }
    auto* p_player = reg().try_get<PlayerPtr>(cit->second.entity());
    if (nullptr == p_player)
    {
        return;
    }
    auto& player = (*p_player);
    l2ms::LeaveGameRequest ms_request;
    ms_request.set_guid(player->PlayingId());
    l2ms_login_stub_.CallMethod(ms_request,
        &l2ms::LoginService_Stub::LeaveGame);
    ErasePlayer(cit);
}

void LoginServiceImpl::Disconnect(::google::protobuf::RpcController* controller, 
    const ::gw2l::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    ClosurePtr cp(done);
    auto cit = connection_accounts_.find(request->connection_id());
    if (cit == connection_accounts_.end())//连接并没有登录
    {
        return;
    }
    //连接已经登录过
    auto* p_player = reg().try_get<PlayerPtr>(cit->second.entity());
    if (nullptr == p_player)
    {
        return;
    }
    auto& player = (*p_player);
    l2ms::DisconnectRequest ms_disconnect;
    ms_disconnect.set_guid(player->PlayingId());
    l2ms_login_stub_.CallMethod(ms_disconnect,
        &l2ms::LoginService_Stub::Disconect);
    ErasePlayer(cit);

}

void LoginServiceImpl::UpdateAccount(const std::string& a, const ::account_database& a_d)
{
    auto it = login_players_.find(a);
    if (it == login_players_.end())
    {
        return;
    }
    auto& ap = it->second;
    ap->set_account_data(a_d);
    ap->OnDbLoaded();
}

void LoginServiceImpl::ErasePlayer(ConnectionEntityMap::iterator& cit)
{
    auto* p_acnt = reg().try_get<std::string>(cit->second.entity());
    if (nullptr != p_acnt)
    {
        login_players_.erase(*p_acnt);
    }
    connection_accounts_.erase(cit);
}

}  // namespace gw2l
