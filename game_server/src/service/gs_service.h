#pragma once
#include "gs_service.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

class GsServiceImpl : public gsservice::GsService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterGs(::google::protobuf::RpcController* controller,
        const gsservice::EnterGsRequest* request,
        gsservice::EnterGsRespone* response,
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

    void MsSend2PlayerViaGs(::google::protobuf::RpcController* controller,
        const gsservice::MsSend2PlayerViaGsRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};