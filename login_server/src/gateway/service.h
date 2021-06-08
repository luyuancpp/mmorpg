#ifndef SRC_GATEWAY_SERVICE_SERVICE_H_
#define SRC_GATEWAY_SERVICE_SERVICE_H_

#ifdef __linux__
#include <unistd.h>
#endif//__linux__

#include "gw2l.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/protorpc/RpcServer.h"

#include "src/database/rpcclient/database_rpcclient.h"

using namespace muduo;
using namespace muduo::net;

namespace gw2l
{
    class LoginServiceImpl : public LoginService
    {
    public:
        virtual void Login(::google::protobuf::RpcController* controller,
            const gw2l::LoginRequest* request,
            gw2l::LoginResponse* response,
            ::google::protobuf::Closure* done)override
        {
            done->Run();

            l2db::LoginRequest db_request;
            db_request.set_account(request->account());
            db_server.Login(db_request);
        }
    };

}  // namespace gw2l

#endif // SRC_GATEWAY_SERVICE_SERVICE_H_