#ifndef GAME_SERVER_SRC_SERVICE_GW2GS_H_
#define GAME_SERVER_SRC_SERVICE_GW2GS_H_
#include "gw2gs.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace gw2gs{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class Gw2gsServiceImpl : public Gw2gsService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void PlayerService(::google::protobuf::RpcController* controller,
        const gw2gs::RpcClientRequest* request,
        gw2gs::RpcClientResponse* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace gw2gs
#endif//GAME_SERVER_SRC_SERVICE_GW2GS_H_
