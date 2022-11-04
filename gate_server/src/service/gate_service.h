#pragma once
#include "gate_service.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class GateServiceImpl : public gwservice::GateService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void StartGS(::google::protobuf::RpcController* controller,
        const gwservice::StartGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void StopGS(::google::protobuf::RpcController* controller,
        const gwservice::StopGSRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void PlayerEnterGs(::google::protobuf::RpcController* controller,
        const gwservice::PlayerEnterGsRequest* request,
        gwservice::PlayerEnterGsResponese* response,
        ::google::protobuf::Closure* done)override;

    void PlayerMessage(::google::protobuf::RpcController* controller,
        const gwservice::PlayerMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void KickConnByController(::google::protobuf::RpcController* controller,
        const gwservice::KickConnRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};