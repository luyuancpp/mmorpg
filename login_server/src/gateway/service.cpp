#include "service.h"

#include "muduo/base/Logging.h"
#include "src/game_rpc/game_rpc_server.h"

#include "src/database/rpcclient/database_rpcclient.h"
#include "src/return_code/return_notice_code.h"
#include "src/return_code/notice_struct.h"

using namespace muduo;
using namespace muduo::net;
using namespace  login;

namespace gw2l
{
    LoginServiceImpl::LoginServiceImpl(LoginStubl2ms& master_login_stub,
        LoginStubl2db& db_login_stub)
        : master_login_stub_(master_login_stub),
          db_login_stub_(db_login_stub)
    {

    }

void LoginServiceImpl::Login(::google::protobuf::RpcController* controller,
    const gw2l::LoginRequest* request,
    gw2l::LoginResponse* response,
    ::google::protobuf::Closure* done)
{
    // login process
    // check account rule
    {
        auto it = login_players_.find(request->account());
        if (it == login_players_.end())
        {
            ::account_database account_data;
            PlayerPtr player(std::make_shared<AccountPlayer>());
            assert(connection_accounts_.find(request->connection_id()) == connection_accounts_.end());
            auto ret = login_players_.emplace(request->account(), player);
            it = ret.first;
        }
        assert(it != login_players_.end());
        auto& player = it->second;
        CheckReturnCloseureError(player->Login());
        if (connection_accounts_.find(request->connection_id()) == connection_accounts_.end())
        {
            connection_accounts_.emplace(request->connection_id(), player);
        }        
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
    cp->s_reqst_.set_account(request->account());
    cp->s_reqst_.set_password(request->password());
    db_login_stub_.CallMethodString(this, &LoginServiceImpl::DbLoginReplied, cp,  &l2db::LoginService_Stub::Login);
}

void LoginServiceImpl::DbLoginReplied(LoginRP d)
{
    d->c_resp_->mutable_account_player()->CopyFrom(d->s_resp_->account_player());
    UpdateAccount(d->s_reqst_.account(), d->s_resp_->account_player());
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
    db_login_stub_.CallMethodString(this,
        &LoginServiceImpl::DbCreatePlayerReplied,
        cp,
        &l2db::LoginService_Stub::CreatePlayer);
}

void LoginServiceImpl::DbCreatePlayerReplied(CreatePlayerRP d)
{
    d->c_resp_->mutable_account_player()->CopyFrom(d->s_resp_->account_player());
    UpdateAccount(d->s_reqst_.account(), d->s_resp_->account_player());
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ::gw2l::EnterGameRequest* request,
    ::gw2l::EnterGameResponse* response,
    ::google::protobuf::Closure* done)
{
    auto cit = connection_accounts_.find(request->connection_id());
    if (cit == connection_accounts_.end())
    {
        ReturnCloseureError(common::RET_LOGIN_CREATE_PLAYER_CONNECTION_HAS_NOT_ACCOUNT);
    }
    auto& ap = cit->second;
    CheckReturnCloseureError(ap->EnterGame());

    // long time in login processing
    if (!ap->IsPlayerId(request->player_id()))
    {
        ReturnCloseureError(common::RET_LOGIN_ENTER_GAME_PLAYER_ID);
    }

    // player in redis return ok
    player_database new_player;
    new_player.set_player_id(request->player_id());
    redis_->Load(new_player, new_player.player_id());
    if (new_player.register_time() > 0)
    {
        EnterMasterServer(request->player_id(), ap->account());
        ReturnCloseureOK;
    }
    // database to redis 
    EnterGameRP cp(std::make_shared<EnterGameRpcString>(response, done));
    cp->s_reqst_.set_account(ap->account());
    cp->s_reqst_.set_player_id(request->player_id());
    db_login_stub_.CallMethodString(this,
        &LoginServiceImpl::EnterGameDbReplied,
        cp,
        &l2db::LoginService_Stub::EnterGame);
}

void LoginServiceImpl::EnterGameDbReplied(EnterGameRP d)
{
    auto cit = login_players_.find(d->s_reqst_.account());
    if (cit == login_players_.end())
    {
        LOG_ERROR << "disconnect not found connection id " << d->s_reqst_.account();
        return;
    }
    auto& ap = cit->second;
    ap->Playing(d->s_reqst_.player_id());

    EnterMasterServer(d->s_reqst_.player_id(), d->s_reqst_.account());
}

void LoginServiceImpl::EnterMasterServer(common::GameGuid player_id, const std::string& account)
{

    EnterMasterGameRC cp(std::make_shared<EnterMasterGameRpcClosure>());
    cp->s_reqst_.set_account(account);
    cp->s_reqst_.set_player_id(player_id);
    master_login_stub_.CallMethodString(this,
        &LoginServiceImpl::EnterMasterGameReplied,
        cp,
        &l2ms::LoginService_Stub::EnterGame);
}

void LoginServiceImpl::EnterMasterGameReplied(EnterMasterGameRC d)
{
   
}

void LoginServiceImpl::Disconnect(::google::protobuf::RpcController* controller, 
    const ::gw2l::DisconnectRequest* request,
    ::gw2l::DisconnectResponse* response,
    ::google::protobuf::Closure* done)
{
    auto cit = connection_accounts_.find(request->connection_id());
    if (cit == connection_accounts_.end())
    {
        LOG_ERROR << "disconnect not found connection id " << std::to_string(request->connection_id());
        return;
    }
    connection_accounts_.erase(cit);
    done->Run();
}

void LoginServiceImpl::UpdateAccount(const std::string& a, const ::account_database& a_d)
{
    auto it = login_players_.find(a);
    if (it == login_players_.end())
    {
        LOG_ERROR << "account empty " << a;
        return;
    }
    auto& ap = it->second;
    ap->set_account_data(a_d);
    ap->OnDbLoaded();
}

}  // namespace gw2l
