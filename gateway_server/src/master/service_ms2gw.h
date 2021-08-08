#ifndef GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_
#define GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_

#include "ms2gw.pb.h"

namespace gateway
{
    class GatewayServer;
}//namespace gateway

namespace ms2gw
{
    class Ms2gwServiceImpl : public Ms2gwService
    {
    public:
        virtual void StartGameServer(::google::protobuf::RpcController* controller,
            const ::ms2gw::StartGameServerRequest* request,
            google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
        virtual void StopGameServer(::google::protobuf::RpcController* controller,
            const ::ms2gw::StopGameServerRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
        virtual void PlayerEnterGameServer(::google::protobuf::RpcController* controller,
            const ::ms2gw::PlayerEnterGameServerRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
        virtual void ConnectMasterFinish(::google::protobuf::RpcController* controller,
            const ::google::protobuf::Empty* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
    private:
    };
}//namespace gw2ms

#endif//GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_
