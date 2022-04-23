#ifndef GATEWAY_SERVER_SRC_SERVICE_GW_NODE_H_
#define GATEWAY_SERVER_SRC_SERVICE_GW_NODE_H_
#include "gw_node.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace gwservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class GwNodeServiceImpl : public GwNodeService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void StartGS(::google::protobuf::RpcController* controller,
        const gwservice::StartGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void StopGS(::google::protobuf::RpcController* controller,
        const gwservice::StopGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void PlayerEnterGs(::google::protobuf::RpcController* controller,
        const gwservice::PlayerEnterGsRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void PlayerMessage(::google::protobuf::RpcController* controller,
        const gwservice::PlayerMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void GsPlayerService(::google::protobuf::RpcController* controller,
        const gwservice::GsPlayerMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace gwservice
#endif//GATEWAY_SERVER_SRC_SERVICE_GW_NODE_H_
