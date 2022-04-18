#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_NORMAL_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_NORMAL_H_
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
    void EnterScene(::google::protobuf::RpcController* controller,
        const normal::SceneTestRequest* request,
        normal::SceneTestResponse* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace normal
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_NORMAL_H_
