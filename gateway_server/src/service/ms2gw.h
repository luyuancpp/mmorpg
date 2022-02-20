#ifndef GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_
#define GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_

#include "ms2gw.pb.h"
///<<< BEGIN WRITING YOUR CODE
namespace gateway
{
    class GatewayServer;
}//namespace gateway
///<<< END WRITING YOUR CODE
namespace ms2gw
{
    ///<<< BEGIN WRITING YOUR CODE
    ///<<< END WRITING YOUR CODE
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
		///<<< BEGIN WRITING YOUR CODE
		///<<< END WRITING YOUR CODE
    };
}//namespace gw2ms

#endif//GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_
