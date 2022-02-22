#ifndef GATEWAY_SERVER_SRC_SERVICE_MS2GW_H_
#define GATEWAY_SERVER_SRC_SERVICE_MS2GW_H_
#include "ms2gw.pb.h"
#include "ms2gw.pb.h"
///<<< BEGIN WRITING YOUR CODE
namespace gateway
{
    class GatewayServer;
}//namespace gateway
///<<< END WRITING YOUR CODE
namespace ms2gw{
 ///<<< BEGIN WRITING YOUR CODE
 ///<<< END WRITING YOUR CODE
class Ms2gwServiceImpl : public Ms2gwService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void StartGS(::google::protobuf::RpcController* controller,
        const ms2gw::StartGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void StopGS(::google::protobuf::RpcController* controller,
        const ms2gw::StopGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void PlayerEnterGS(::google::protobuf::RpcController* controller,
        const ms2gw::PlayerEnterGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
};
}// namespace ms2gw
#endif//GATEWAY_SERVER_SRC_SERVICE_MS2GW_H_
