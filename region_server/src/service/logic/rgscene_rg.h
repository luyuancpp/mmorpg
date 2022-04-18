#ifndef REGION_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_RG_H_
#define REGION_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_RG_H_
#include "src/pb/pbc/logic_proto/scene_rg.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace regionservcie{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class RgServiceImpl : public RgService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void StartCrossMainGS(::google::protobuf::RpcController* controller,
        const regionservcie::StartCrossMainGSRequest* request,
        regionservcie::StartCrossMainGSResponse* response,
        ::google::protobuf::Closure* done)override;

    void StartCrossRoomGS(::google::protobuf::RpcController* controller,
        const regionservcie::StartCrossRoomGSRequest* request,
        regionservcie::StartCrossRoomGSResponse* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace regionservcie
#endif//REGION_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_RG_H_
