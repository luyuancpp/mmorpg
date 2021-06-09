#include "service.h"

#include "gw2l.pb.h"
#include "l2db.pb.h"

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
    using LoginResponParam = ClosureParam<l2db::LoginRequest, 
        l2db::LoginResponse, 
        gw2l::LoginResponse>;
    LoginResponParam* s = new LoginResponParam{ response, done};
    DbRpcClient::s().db_client()->SendRequest1(this, s, &LoginServiceImpl::DbLoginReplied,
        &l2db::LoginService_Stub::Login);
}

}  // namespace gw2l
