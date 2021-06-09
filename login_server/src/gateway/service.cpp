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
    DbRpcClient::s().SendRequest1(
        this, new LoginResponParam{ response, done }, 
        &LoginServiceImpl::DbLoginReplied,
        &l2db::LoginService_Stub::Login);
}

}  // namespace gw2l
