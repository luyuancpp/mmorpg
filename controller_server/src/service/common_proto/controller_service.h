#pragma once
#include "controller_service.pb.h"
class ControllerServiceImpl : public ::ControllerService{
public:
public:
    void StartGs(::google::protobuf::RpcController* controller,
        const ::CtrlStartGsRequest* request,
        ::CtrlStartGsResponse* response,
        ::google::protobuf::Closure* done)override;

    void OnGateConnect(::google::protobuf::RpcController* controller,
        const ::GateConnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGateLeaveGame(::google::protobuf::RpcController* controller,
        const ::GateLeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGatePlayerService(::google::protobuf::RpcController* controller,
        const ::GateClientMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGateDisconnect(::google::protobuf::RpcController* controller,
        const ::GateDisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnLsLoginAccount(::google::protobuf::RpcController* controller,
        const ::CtrlLoginAccountRequest* request,
        ::CtrlLoginAccountResponse* response,
        ::google::protobuf::Closure* done)override;

    void OnLsEnterGame(::google::protobuf::RpcController* controller,
        const ::CtrlEnterGameRequest* request,
        ::CtrlEnterGameResponese* response,
        ::google::protobuf::Closure* done)override;

    void OnLsLeaveGame(::google::protobuf::RpcController* controller,
        const ::CtrlLsLeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnLsDisconnect(::google::protobuf::RpcController* controller,
        const ::CtrlLsDisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGsPlayerService(::google::protobuf::RpcController* controller,
        const ::NodeServiceMessageRequest* request,
        ::NodeServiceMessageResponse* response,
        ::google::protobuf::Closure* done)override;

    void AddCrossServerScene(::google::protobuf::RpcController* controller,
        const ::AddCrossServerSceneRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void EnterGsSucceed(::google::protobuf::RpcController* controller,
        const ::EnterGsSucceedRequest* request,
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