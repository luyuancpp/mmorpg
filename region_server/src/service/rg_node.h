#ifndef REGION_SERVER_SRC_SERVICE_./RG_PROTO/RG_NODE_H_
#define REGION_SERVER_SRC_SERVICE_./RG_PROTO/RG_NODE_H_
#include "rg_node.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace regionservcie{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class G2rgServiceImpl : public G2rgService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void StartCrossMainGS(::google::protobuf::RpcController* controller,
        const regionservcie::StartGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void StartCrossRoomGS(::google::protobuf::RpcController* controller,
        const regionservcie::StartGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace regionservcie
#endif//REGION_SERVER_SRC_SERVICE_./RG_PROTO/RG_NODE_H_
