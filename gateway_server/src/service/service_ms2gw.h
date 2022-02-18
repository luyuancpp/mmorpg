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
        virtual void StartGS(::google::protobuf::RpcController* controller,
            const ::ms2gw::StartGSRequest* request,
            google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
        virtual void StopGS(::google::protobuf::RpcController* controller,
            const ::ms2gw::StopGSRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
        virtual void PlayerEnterGS(::google::protobuf::RpcController* controller,
            const ::ms2gw::PlayerEnterGSRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;

    private:
    };
}//namespace gw2ms

#endif//GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_
