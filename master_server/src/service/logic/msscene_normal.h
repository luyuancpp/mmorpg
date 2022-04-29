#ifndef MASTER_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_NORMAL_H_
#define MASTER_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_NORMAL_H_
#include "src/pb/pbc/logic_proto/scene_normal.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace normal{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class ServerSceneServiceImpl : public ServerSceneService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void UpdateCrossMainSceneInfo(::google::protobuf::RpcController* controller,
        const normal::UpdateCrossMainSceneInfoRequest* request,
        normal::UpdateCrossMainSceneInfoResponse* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace normal
#endif//MASTER_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_NORMAL_H_
