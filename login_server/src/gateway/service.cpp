#include "service.h"

#include "muduo/base/Logging.h"
#include "muduo/net/protorpc/RpcServer.h"

#include "src/database/rpcclient/database_rpcclient.h"

#include "l2db.pb.h"

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
    ::account_database response_account_database;
    redis_->Load(response_account_database, request->account());
    if (!response_account_database.password().empty())
    {
        response->mutable_account_player()->CopyFrom(response_account_database);
        done->Run();
        return;
    }

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
    response->mutable_account_player()->mutable_simple_players()->add_players()->set_player_id(1);
    done->Run();
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ::gw2l::EnterGameRequest* request,
    ::gw2l::EnterGameRequest* response,
    ::google::protobuf::Closure* done)
{
    done->Run();
}

}  // namespace gw2l
