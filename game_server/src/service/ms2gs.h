#ifndef GAME_SERVER_SRC_SERVICE_MS2GS_H_
#define GAME_SERVER_SRC_SERVICE_MS2GS_H_
#include "ms2gs.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace ms2gs{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class Ms2gServiceImpl : public Ms2gService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterGame(::google::protobuf::RpcController* controller,
        const ms2gs::EnterGameRequest* request,
        ms2gs::EnterGameRespone* response,
        ::google::protobuf::Closure* done)override;

    void PlayerService(::google::protobuf::RpcController* controller,
        const ms2gs::MsPlayerMessageRequest* request,
        ms2gs::MsPlayerMessageRespone* response,
        ::google::protobuf::Closure* done)override;

    void PlayerServiceNoRespone(::google::protobuf::RpcController* controller,
        const ms2gs::MsPlayerMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace ms2gs
#endif//GAME_SERVER_SRC_SERVICE_MS2GS_H_
