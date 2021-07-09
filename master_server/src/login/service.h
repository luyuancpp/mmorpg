#ifndef MASTER_SERVER_SRC_LOGIN_SERVICE_H_
#define MASTER_SERVER_SRC_LOGIN_SERVICE_H_

#include "l2ms.pb.h"

namespace l2ms
{
    class LoginServiceImpl : public l2ms::LoginService
    {
    public:
        virtual void EnterGame(::google::protobuf::RpcController* controller,
            const ::l2ms::EnterGameRequest* request,
            ::l2ms::EnterGameResponse* response,
            ::google::protobuf::Closure* done)override;

        virtual void LeaveGame(::google::protobuf::RpcController* controller,
            const ::l2ms::LeaveGameRequest* request,
            ::l2ms::LeaveGameResponse* response,
            ::google::protobuf::Closure* done)override;

        virtual void Disconect(::google::protobuf::RpcController* controller,
            const ::l2ms::DisconectRequest* request,
            ::l2ms::DisconectResponse* response,
            ::google::protobuf::Closure* done)override;
    private:

    };
}

#endif//MASTER_SERVER_SRC_LOGIN_SERVICE_H_
