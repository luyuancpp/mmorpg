#ifndef MASTER_SERVER_SRC_GATEWAY_SERVICE_GW2MS_H_
#define MASTER_SERVER_SRC_GATEWAY_SERVICE_GW2MS_H_

#include "gw2ms.pb.h"
///<<< BEGIN WRITING YOUR CODE
namespace master
{
    class MasterServer;
}//master
///<<< END WRITING YOUR CODE
namespace gw2ms
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
    class Gw2msServiceImpl : public gw2ms::Gw2msService
    {
    public:
        virtual void GwConnectMaster(::google::protobuf::RpcController* controller,
            const ::gw2ms::ConnectRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;

        virtual void PlayerDisconnect(::google::protobuf::RpcController* controller,
            const ::gw2ms::PlayerDisconnectRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;

        virtual void LeaveGame(::google::protobuf::RpcController* controller,
            const ::gw2ms::LeaveGameRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
		///<<< BEGIN WRITING YOUR CODE
		///<<< END WRITING YOUR CODE

    };
}//namespace gw2ms

#endif//MASTER_SERVER_SRC_GATEWAY_SERVICE_GW2MS_H_
