#ifndef GAME_SERVER_SRC_SERVICE_GS_NODE_H_
#define GAME_SERVER_SRC_SERVICE_GS_NODE_H_
#include "gs_node.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace gsservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class GsServiceImpl : public GsService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterGame(::google::protobuf::RpcController* controller,
        const gsservice::EnterGameRequest* request,
        gsservice::EnterGameRespone* response,
        ::google::protobuf::Closure* done)override;

    void PlayerService(::google::protobuf::RpcController* controller,
        const gsservice::MsPlayerMessageRequest* request,
        gsservice::MsPlayerMessageRespone* response,
        ::google::protobuf::Closure* done)override;

    void PlayerServiceNoRespone(::google::protobuf::RpcController* controller,
        const gsservice::MsPlayerMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void GwPlayerService(::google::protobuf::RpcController* controller,
        const gsservice::RpcClientRequest* request,
        gsservice::RpcClientResponse* response,
        ::google::protobuf::Closure* done)override;

    void Disconnect(::google::protobuf::RpcController* controller,
        const gsservice::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void GwConnectGs(::google::protobuf::RpcController* controller,
        const gsservice::ConnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void RgEnterRoom(::google::protobuf::RpcController* controller,
        const gsservice::Rg2gTest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace gsservice
#endif//GAME_SERVER_SRC_SERVICE_GS_NODE_H_
