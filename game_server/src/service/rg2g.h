#ifndef GAME_SERVER_SRC_SERVICE_RG2G_H_
#define GAME_SERVER_SRC_SERVICE_RG2G_H_
#include "rg2g.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace rg2g{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class Rg2gServiceImpl : public Rg2gService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterRoom(::google::protobuf::RpcController* controller,
        const rg2g::Rg2gTest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace rg2g
#endif//GAME_SERVER_SRC_SERVICE_RG2G_H_
