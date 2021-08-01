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
        Ms2gwServiceImpl(gateway::GatewayServer* server)
            : server_(server)
        {}

        virtual void StartLogicServer(::google::protobuf::RpcController* controller,
            const ::ms2gw::StartLogicServerRequest* request,
            google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
        virtual void StopLogicServer(::google::protobuf::RpcController* controller,
            const ::ms2gw::StopLogicServerRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
        virtual void PlayerEnterGameServer(::google::protobuf::RpcController* controller,
            const ::ms2gw::PlayerEnterGameServerRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
    private:
        gateway::GatewayServer* server_{ nullptr };
    };
}//namespace gw2ms

#endif//GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_
