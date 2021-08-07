#include "service_gw2l.h"

#include "muduo/base/Logging.h"
#include "src/server_common/rpc_server.h"
#include "src/return_code/return_notice_code.h"
#include "src/server_common/closure_auto_done.h"

using namespace muduo;
using namespace muduo::net;

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
    //只连接不登录,占用连接
    // login process
    // check account rule
    //check string rule
    {
        auto it = login_players_.find(request->account());
        if (it == login_players_.end())
        {
            assert(connection_accounts_.find(request->connection_id()) == connection_accounts_.end());
            auto ret = login_players_.emplace(request->account(), std::make_shared<AccountPlayer>());
            it = ret.first;
        }
        auto& player = it->second;
        CheckReturnCloseureError(player->Login());
        connection_accounts_.emplace(request->connection_id(), player);
        auto& account_data = player->account_data();
        redis_->Load(account_data, request->account());
        if (!account_data.password().empty())
        {
            response->mutable_account_player()->CopyFrom(account_data);
            player->OnDbLoaded();
            ReturnCloseureOK;
        }
    }
 
    // database process
    LoginRP cp(std::make_shared<LoginRpcString>(response, done));
    auto& s_reqst = cp->s_reqst_;
    s_reqst.set_account(request->account());
    s_reqst.set_password(request->password());
    l2db_login_stub_.CallMethodString(this, &LoginServiceImpl::DbLoginReplied, cp,  &l2db::LoginService_Stub::Login);
}

void LoginServiceImpl::DbLoginReplied(LoginRP d)
{
    auto& sresp = d->s_resp_;
    d->c_resp_->mutable_account_player()->CopyFrom(sresp->account_player());
    UpdateAccount(d->s_reqst_.account(), sresp->account_player());
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
        ReturnCloseureError(common::RET_LOGIN_CREATE_PLAYER_CONNECTION_HAS_NOT_ACCOUNT);
    }
    auto& ap = cit->second;
    CheckReturnCloseureError(ap->CreatePlayer());

    // database process
    CreatePlayerRP cp(std::make_shared<CreatePlayerRpcString>(response, done));
    cp->s_reqst_.set_account(cit->second->account());
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
    auto player_id = request->player_id();
    auto connection_id = request->connection_id();
    auto cit = connection_accounts_.find(connection_id);
    if (cit == connection_accounts_.end())
    {
        ReturnCloseureError(common::RET_LOGIN_CREATE_PLAYER_CONNECTION_HAS_NOT_ACCOUNT);
    }
    auto& ap = cit->second;
    // check second times change player id error 
    CheckReturnCloseureError(ap->EnterGame());

    // long time in login processing
    if (!ap->IsPlayerId(player_id))
    {
        ReturnCloseureError(common::RET_LOGIN_ENTER_GAME_PLAYER_ID);
    }
    auto& account = ap->account();
    // player in redis return ok
    player_database new_player;
    redis_->Load(new_player, player_id);
    ap->Playing(player_id);//test
    response->set_connection_id(connection_id);
    response->set_player_id(player_id);//test
    if (new_player.player_id() > 0)
    {
        EnterMasterServer(account, response, done);
        return;
    }        
    // database to redis 
    EnterGameDbRP cp(std::make_shared<EnterGameDbRpcString>(response, done));
    auto& sreqst = cp->s_reqst_;
    sreqst.set_account(account);
    sreqst.set_player_id(player_id);
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
    EnterMasterServer(sreqst.account(), response, done);
}

void LoginServiceImpl::EnterGameMasterReplied(EnterGameMasterRP d)
{
    d->c_resp_->set_game_server_id(d->s_resp_->game_server_id());
}

void LoginServiceImpl::EnterMasterServer(const std::string& account,
    ::gw2l::EnterGameResponse* response,
    ::google::protobuf::Closure* done)
{   
    EnterGameMasterRP cp(std::make_shared<EnterGameMasterRpcString>(response, done));
    cp->s_reqst_.set_account(account);
    cp->s_reqst_.set_player_id(response->player_id());
    cp->s_reqst_.set_connection_id(response->connection_id());
    l2ms_login_stub_.CallMethodString(this,
        &LoginServiceImpl::EnterGameMasterReplied,
        cp,
        &l2ms::LoginService_Stub::EnterGame);
}

void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller, 
    const ::gw2l::LeaveGameRequest* request, 
    ::google::protobuf::Empty* response, 
    ::google::protobuf::Closure* done)
{
    common::ClosurePtr cp(done);
    //连接过，登录过
    auto cit = connection_accounts_.find(request->connection_id());
    if (cit == connection_accounts_.end())
    {
        LOG_ERROR << " leave game not found connection";
        return;
    }
    auto& player = cit->second;
    l2ms::LeaveGameRequest ms_request;
    ms_request.set_player_id(player->PlayingId());
    l2ms_login_stub_.CallMethod(ms_request,
        &l2ms::LoginService_Stub::LeaveGame);
    ErasePlayer(cit);
    if (connection_accounts_.empty() && login_players_.empty())
    {
        LOG_INFO << "player empty";
    }
}

void LoginServiceImpl::Disconnect(::google::protobuf::RpcController* controller, 
    const ::gw2l::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    auto cit = connection_accounts_.find(request->connection_id());
    if (cit == connection_accounts_.end())//连接并没有登录
    {
        return;
    }
    //连接已经登录过
    auto& player = cit->second;
    l2ms::DisconnectRequest ms_disconnect;
    ms_disconnect.set_player_id(player->PlayingId());
    l2ms_login_stub_.CallMethod(ms_disconnect,
        &l2ms::LoginService_Stub::Disconect);
    ErasePlayer(cit);
    if (connection_accounts_.empty() && login_players_.empty())
    {
        LOG_INFO << "player empty";
    }
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

}  // namespace gw2l
