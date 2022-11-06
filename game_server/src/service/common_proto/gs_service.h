#pragma once
#include "game_service.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include <unordered_map>
///<<< END WRITING YOUR CODE
class GsServiceImpl : public gsservice::GsService{
public:
///<<< BEGIN WRITING YOUR CODE
private:
///<<< END WRITING YOUR CODE
public:
    void EnterGs(::google::protobuf::RpcController* controller,
        const gsservice::EnterGsRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void PlayerService(::google::protobuf::RpcController* controller,
        const gsservice::ControllerPlayerMessageRequest* request,
        gsservice::ControllerPlayerMessageRespone* response,
        ::google::protobuf::Closure* done)override;

    void GatePlayerService(::google::protobuf::RpcController* controller,
        const gsservice::RpcClientRequest* request,
        gsservice::RpcClientResponse* response,
        ::google::protobuf::Closure* done)override;

    void Disconnect(::google::protobuf::RpcController* controller,
        const gsservice::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void GateConnectGs(::google::protobuf::RpcController* controller,
        const gsservice::ConnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void ControllerSend2PlayerViaGs(::google::protobuf::RpcController* controller,
        const gsservice::ControllerSend2PlayerViaGsRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};