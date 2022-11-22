#pragma once
#include "game_service.pb.h"
class GsServiceImpl : public gsservice::GsService{
public:
public:
    void EnterGs(::google::protobuf::RpcController* controller,
        const gsservice::EnterGsRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void Send2PlayerService(::google::protobuf::RpcController* controller,
        const ::NodeServiceMessageRequest* request,
        ::NodeServiceMessageResponse* response,
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
        const ::NodeServiceMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};