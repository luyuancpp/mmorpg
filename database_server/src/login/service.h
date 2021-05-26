#ifndef SRC_LOGIN_SERVICE_SERVICE_H_
#define SRC_LOGIN_SERVICE_SERVICE_H_

#ifdef __linux__
#include <unistd.h>
#endif//__linux__

#include "l2db.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/protorpc/RpcServer.h"

using namespace muduo;
using namespace muduo::net;

namespace l2db
{
    class LoginServiceImpl : public LoginService
    {
    public:
        virtual void Login(::google::protobuf::RpcController* controller,
            const l2db::LoginRequest* request,
            l2db::LoginResponse* response,
            ::google::protobuf::Closure* done)override
        {
            done->Run();
        }
    };

}  // namespace l2db

#endif // SRC_LOGIN_SERVICE_SERVICE_H_