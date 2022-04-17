#ifndef REGION_SERVER_SRC_SERVICE_RG_NODE_H_
#define REGION_SERVER_SRC_SERVICE_RG_NODE_H_
#include "rg_node.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace rgservcie{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class RgServiceImpl : public RgService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void StartCrossMainGS(::google::protobuf::RpcController* controller,
        const rgservcie::StartMainRoomGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void StartCrossRoomGS(::google::protobuf::RpcController* controller,
        const rgservcie::StartCrossRoomGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void StartMs(::google::protobuf::RpcController* controller,
        const rgservcie::StartMsRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace rgservcie
#endif//REGION_SERVER_SRC_SERVICE_RG_NODE_H_
