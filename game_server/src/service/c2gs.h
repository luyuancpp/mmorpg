#ifndef GAME_SERVER_SRC_SERVICE_C2GS_H_
#define GAME_SERVER_SRC_SERVICE_C2GS_H_
#include "c2gs.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace c2gs{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class C2GsServiceImpl : public C2GsService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(::google::protobuf::RpcController* controller,
        const c2gs::EnterSceneRequest* request,
        c2gs::EnterSceneResponse* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace c2gs
#endif//GAME_SERVER_SRC_SERVICE_C2GS_H_
