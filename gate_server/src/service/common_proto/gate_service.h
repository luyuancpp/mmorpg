#pragma once
#include "gate_service.pb.h"
class GateServiceImpl : public gateservice::GateService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void StartGS(::google::protobuf::RpcController* controller,
        const gateservice::StartGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void StopGS(::google::protobuf::RpcController* controller,
        const gateservice::StopGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void PlayerEnterGs(::google::protobuf::RpcController* controller,
        const gateservice::PlayerEnterGsRequest* request,
        gateservice::PlayerEnterGsResponese* response,
        ::google::protobuf::Closure* done)override;

    void PlayerMessage(::google::protobuf::RpcController* controller,
        const gateservice::PlayerMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void KickConnByController(::google::protobuf::RpcController* controller,
        const gateservice::KickConnRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};