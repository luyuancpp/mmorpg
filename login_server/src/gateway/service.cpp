#include "service.h"

#include "muduo/base/Logging.h"
#include "muduo/net/protorpc/RpcServer.h"

#include "src/database/rpcclient/database_rpcclient.h"

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
    auto cp = new LoginResponParam{ response, done };
    cp->server_request_.set_account(request->account());
    cp->server_request_.set_password(request->password());
    DbRpcClient::s().SendRequest1(
        this, 
        cp,
        &LoginServiceImpl::DbLoginReplied,
        &l2db::LoginService_Stub::Login);
}

void LoginServiceImpl::DbLoginReplied(LoginResponParam* respone)
{
     std::unique_ptr<LoginResponParam> d(respone);
     d->client_respone_->set_account(d->server_respone_->player_account().account());
     d->client_respone_->set_password(d->server_respone_->player_account().password());
}

}  // namespace gw2l
