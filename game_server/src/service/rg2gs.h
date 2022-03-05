#ifndef GAME_SERVER_SRC_SERVICE_RG2GS_H_
#define GAME_SERVER_SRC_SERVICE_RG2GS_H_
#include "rg2gs.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace rg2gs{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class Rg2gServiceImpl : public Rg2gService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterRoom(::google::protobuf::RpcController* controller,
        const rg2gs::Rg2gTest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace rg2gs
#endif//GAME_SERVER_SRC_SERVICE_RG2GS_H_
