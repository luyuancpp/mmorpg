#ifndef MASTER_SERVER_SRC_LOGIN_SERVICE_H_
#define MASTER_SERVER_SRC_LOGIN_SERVICE_H_

#include "l2ms.pb.h"

namespace l2ms
{
    class LoginServiceImpl : public l2ms::LoginService
    {
    public:

        virtual void Login(::google::protobuf::RpcController* controller,
            const ::l2ms::LoginRequest* request,
            ::l2ms::LoginResponse* response,
            ::google::protobuf::Closure* done);

        virtual void EnterGame(::google::protobuf::RpcController* controller,
            const ::l2ms::EnterGameRequest* request,
            ::l2ms::EnterGameResponse* response,
            ::google::protobuf::Closure* done)override;

    private:

    };
}

#endif//MASTER_SERVER_SRC_LOGIN_SERVICE_H_
