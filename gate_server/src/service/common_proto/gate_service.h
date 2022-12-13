#pragma once
#include "gate_service.pb.h"
class GateServiceImpl : public gateservice::GateService{
public:
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
        const ::NodeServiceMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void KickConnByController(::google::protobuf::RpcController* controller,
        const gateservice::KickConnRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void RouteNodeStringMsg(::google::protobuf::RpcController* controller,
        const ::RouteMsgStringRequest* request,
        ::RouteMsgStringResponse* response,
        ::google::protobuf::Closure* done)override;

    void RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
        const ::RoutePlayerMsgStringRequest* request,
        ::RoutePlayerMsgStringResponse* response,
        ::google::protobuf::Closure* done)override;

};