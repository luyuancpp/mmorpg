#pragma once
#include "controller_service.pb.h"
class ControllerServiceImpl : public ::ControllerService{
public:
public:
    void StartGs(::google::protobuf::RpcController* controller,
        const ::CtrlStartGsRequest* request,
        ::CtrlStartGsResponse* response,
        ::google::protobuf::Closure* done)override;

    void GateConnect(::google::protobuf::RpcController* controller,
        const ::GateConnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void GateLeaveGame(::google::protobuf::RpcController* controller,
        const ::GateLeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void GatePlayerService(::google::protobuf::RpcController* controller,
        const ::GateClientMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void GateDisconnect(::google::protobuf::RpcController* controller,
        const ::GateDisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void StartLs(::google::protobuf::RpcController* controller,
        const ::StartLsRequest* request,
        ::StartLsResponse* response,
        ::google::protobuf::Closure* done)override;

    void LsLoginAccount(::google::protobuf::RpcController* controller,
        const ::CtrlLoginAccountRequest* request,
        ::CtrlLoginAccountResponse* response,
        ::google::protobuf::Closure* done)override;

    void LsEnterGame(::google::protobuf::RpcController* controller,
        const ::CtrlEnterGameRequest* request,
        ::CtrlEnterGameResponese* response,
        ::google::protobuf::Closure* done)override;

    void LsLeaveGame(::google::protobuf::RpcController* controller,
        const ::CtrlLsLeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void LsDisconnect(::google::protobuf::RpcController* controller,
        const ::CtrlLsDisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void GsPlayerService(::google::protobuf::RpcController* controller,
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