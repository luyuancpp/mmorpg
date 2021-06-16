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
using LoginRpcString = common::RpcString<l2db::LoginRequest,
    l2db::LoginResponse,
    gw2l::LoginResponse>;
using LoginRP = std::shared_ptr<LoginRpcString>;
void DbLoginReplied(LoginRP d)
{
    d->c_resp_->mutable_account_player()->CopyFrom(d->s_resp_->account_player());
}

void LoginServiceImpl::Login(::google::protobuf::RpcController* controller,
    const gw2l::LoginRequest* request,
    gw2l::LoginResponse* response,
    ::google::protobuf::Closure* done)
{
    // login process
    {
        auto it = accounts_.find(request->account());
        if (it != accounts_.end())
        {
            response->mutable_account_player()->CopyFrom(it->second);
            ReturnCloseureError(common::RET_OK);
        }
        else if (it == accounts_.end())
        {
            ::account_database account_data;
            auto ret = accounts_.emplace(request->account(), account_data);
            connection_accounts_.emplace(request->connection_id(), request->account());
            assert(ret.second);
            it = ret.first;
        }
        if (it == accounts_.end())
        {
            ReturnCloseureError(common::RET_LOGIN_CNAT_FIND_ACCOUNT);
        }

        auto& account_data = it->second;
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
    DbRpcClient::s().SendRequest(DbLoginReplied, cp,  &l2db::LoginService_Stub::Login);
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

    auto ait = accounts_.find(cit->second);
    if (ait == accounts_.end())
    {
        ReturnCloseureError(common::RET_LOGIN_CREATE_PLAYER_DONOT_LOAD_ACCOUNT);
    }
    static int32_t kMaxPlayerSize = 4;
    if (ait->second.simple_players().players_size() >= kMaxPlayerSize)
    {
        ReturnCloseureError(common::RET_LOGIN_MAX_PLAYER_SIZE);
    }

    // database process
    CreatePlayerRP cp(std::make_shared<CreatePlayerRpcString>(response, done));
    cp->s_reqst_.set_account(ait->second.account());
    DbRpcClient::s().SendRequest(this,
        &LoginServiceImpl::DbCratePlayerReplied,
        cp,
        &l2db::LoginService_Stub::CratePlayer);
}

void LoginServiceImpl::DbCratePlayerReplied(CreatePlayerRP d)
{
    d->c_resp_->mutable_account_player()->CopyFrom(d->s_resp_->account_player());
    d->c_resp_->set_player_id(d->s_resp_->player_id());
    auto it = accounts_.find(d->s_reqst_.account());
    if (it == accounts_.end())
    {
        std::string msg = std::string("account empty ") + d->s_reqst_.account();
        LOG_ERROR << msg;
        return;
    }
    it->second = d->s_resp_->account_player();
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ::gw2l::EnterGameRequest* request,
    ::gw2l::EnterGameRespone* response,
    ::google::protobuf::Closure* done)
{
    done->Run();
}

}  // namespace gw2l
