#include "service.h"

#include "muduo/base/Logging.h"
#include "muduo/net/protorpc/RpcServer.h"

#include "src/database/rpcclient/database_rpcclient.h"
#include "src/return_code/return_notice_code.h"
#include "src/return_code/notice_struct.h"

using namespace muduo;
using namespace muduo::net;
using namespace  login;

namespace gw2l
{

void LoginServiceImpl::Login(::google::protobuf::RpcController* controller,
    const gw2l::LoginRequest* request,
    gw2l::LoginResponse* response,
    ::google::protobuf::Closure* done)
{
    // login process
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
    DbRpcClient::GetSingleton().SendRequest(this, &LoginServiceImpl::DbLoginReplied, cp,  &l2db::LoginService_Stub::Login);
}

void LoginServiceImpl::DbLoginReplied(LoginRP d)
{
    d->c_resp_->mutable_account_player()->CopyFrom(d->s_resp_->account_player());
    UpdateAccount(d->s_reqst_.account(), d->s_resp_->account_player());
}

void LoginServiceImpl::CreatPlayer(::google::protobuf::RpcController* controller,
    const gw2l::CreatePlayerRequest* request, 
    gw2l::CreatePlayerRespone* response, 
    ::google::protobuf::Closure* done)
{
    // login process

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
    DbRpcClient::GetSingleton().SendRequest(this,
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
    ::gw2l::EnterGameRespone* response,
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
        ReturnCloseureOK;
    }
    // database to redis 
    EnterGameRP cp(std::make_shared<EnterGameRpcString>(response, done));
    cp->s_reqst_.set_account(ap->account());
    cp->s_reqst_.set_player_id(request->player_id());
    DbRpcClient::GetSingleton().SendRequest(this,
        &LoginServiceImpl::EnterGameReplied,
        cp,
        &l2db::LoginService_Stub::EnterGame);
}

void LoginServiceImpl::EnterGameReplied(EnterGameRP d)
{
    auto it = login_players_.find(d->s_reqst_.account());
    if (it == login_players_.end())
    {
        std::string msg = std::string("disconnect not found connection id ") + d->s_reqst_.account();
        LOG_ERROR << msg;
        return;
    }
    it->second->Playing();
}

void LoginServiceImpl::Disconnect(::google::protobuf::RpcController* controller, 
    const ::gw2l::DisconnectRequest* request,
    ::gw2l::DisconnectRespone* response,
    ::google::protobuf::Closure* done)
{
    auto cit = connection_accounts_.find(request->connection_id());
    if (cit == connection_accounts_.end())
    {
        std::string msg("disconnect not found connection id " + std::to_string(request->connection_id()));
        LOG_ERROR << msg;
    }
    connection_accounts_.erase(cit);
    done->Run();
}

void LoginServiceImpl::UpdateAccount(const std::string& a, const ::account_database& a_d)
{
    auto it = login_players_.find(a);
    if (it == login_players_.end())
    {
        std::string msg = std::string("account empty ") + a;
        LOG_ERROR << msg;
        return;
    }
    auto& ap = it->second;
    ap->set_account_data(a_d);
    ap->OnDbLoaded();
}

}  // namespace gw2l
