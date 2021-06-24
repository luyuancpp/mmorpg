#include "service.h"

#include "muduo/base/Logging.h"
#include "muduo/net/protorpc/RpcServer.h"

#include "src/database/rpcclient/database_rpcclient.h"
#include "src/return_code/return_notice_code.h"

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
        if (it != login_players_.end())
        {
            response->mutable_account_player()->CopyFrom(it->second->account_data());
            ReturnCloseureError(common::RET_OK);
        }
        else if (it == login_players_.end())
        {
            ::account_database account_data;
            PlayerPtr player(std::make_shared<LoginPlayer>());
            auto ret = login_players_.emplace(request->account(), player);
            connection_accounts_.emplace(request->connection_id(), request->account());
            assert(ret.second);
            it = ret.first;
        }
        if (it == login_players_.end())
        {
            ReturnCloseureError(common::RET_LOGIN_CNAT_FIND_ACCOUNT);
        }

        auto& account_data = it->second->account_data();
        redis_->Load(account_data, request->account());
        if (!account_data.password().empty())
        {
            response->mutable_account_player()->CopyFrom(account_data);
            ReturnCloseureError(common::RET_OK);
        }
    }
 
    // database process
    LoginRP cp(std::make_shared<LoginRpcString>(response, done));
    cp->s_reqst_.set_account(request->account());
    cp->s_reqst_.set_password(request->password());
    DbRpcClient::s().SendRequest(this, &LoginServiceImpl::DbLoginReplied, cp,  &l2db::LoginService_Stub::Login);
}

void LoginServiceImpl::DbLoginReplied(LoginRP d)
{
    d->c_resp_->mutable_account_player()->CopyFrom(d->s_resp_->account_player());
    UpdateAccount(d->s_reqst_.account(), d->s_resp_->account_player());
}

void LoginServiceImpl::CratePlayer(::google::protobuf::RpcController* controller, 
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

    auto ait = login_players_.find(cit->second);
    if (ait == login_players_.end())
    {
        ReturnCloseureError(common::RET_LOGIN_CREATE_PLAYER_DONOT_LOAD_ACCOUNT);
    }
    
    if (ait->second->IsFullPlayer())
    {
        ReturnCloseureError(common::RET_LOGIN_MAX_PLAYER_SIZE);
    }

    // database process
    CreatePlayerRP cp(std::make_shared<CreatePlayerRpcString>(response, done));
    cp->s_reqst_.set_account(ait->second->account());
    DbRpcClient::s().SendRequest(this,
        &LoginServiceImpl::DbCratePlayerReplied,
        cp,
        &l2db::LoginService_Stub::CratePlayer);
}

void LoginServiceImpl::DbCratePlayerReplied(CreatePlayerRP d)
{
    d->c_resp_->mutable_account_player()->CopyFrom(d->s_resp_->account_player());
    UpdateAccount(d->s_reqst_.account(), d->s_resp_->account_player());
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ::gw2l::EnterGameRequest* request,
    ::gw2l::EnterGameRespone* response,
    ::google::protobuf::Closure* done)
{
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
    it->second->set_account_data(a_d);
}

}  // namespace gw2l
