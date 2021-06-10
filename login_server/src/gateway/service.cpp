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
    d->client_respone_->set_account(d->server_respone_->player_account().account());
    d->client_respone_->set_password(d->server_respone_->player_account().password());
}

void LoginServiceImpl::Login(::google::protobuf::RpcController* controller,
    const gw2l::LoginRequest* request,
    gw2l::LoginResponse* response,
    ::google::protobuf::Closure* done)
{
    LoginRP cp(std::make_shared<LoginRpcString>(response, done));
    cp->server_request_.set_account(request->account());
    cp->server_request_.set_password(request->password());
    DbRpcClient::s().SendRequest(DbLoginReplied, cp,  &l2db::LoginService_Stub::Login);
}

}  // namespace gw2l
