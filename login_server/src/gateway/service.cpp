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
    d->client_respone_->mutable_account_player()->CopyFrom(d->server_respone_->account_player());
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
    cp->server_request_.set_account(request->account());
    cp->server_request_.set_password(request->password());
    DbRpcClient::s().SendRequest(DbLoginReplied, cp,  &l2db::LoginService_Stub::Login);
}

}  // namespace gw2l
