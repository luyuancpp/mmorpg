#pragma once
#include "controller_service.pb.h"
class ControllerNodeServiceImpl : public controllerservice::ControllerNodeService{
public:
public:
    void StartGs(::google::protobuf::RpcController* controller,
        const controllerservice::StartGsRequest* request,
        controllerservice::StartGsResponse* response,
        ::google::protobuf::Closure* done)override;

    void OnGateConnect(::google::protobuf::RpcController* controller,
        const controllerservice::ConnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGateLeaveGame(::google::protobuf::RpcController* controller,
        const controllerservice::LeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGatePlayerService(::google::protobuf::RpcController* controller,
        const controllerservice::ClientMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGateDisconnect(::google::protobuf::RpcController* controller,
        const controllerservice::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnLsLoginAccount(::google::protobuf::RpcController* controller,
        const controllerservice::LoginAccountRequest* request,
        controllerservice::LoginAccountResponse* response,
        ::google::protobuf::Closure* done)override;

    void OnLsEnterGame(::google::protobuf::RpcController* controller,
        const controllerservice::EnterGameRequest* request,
        controllerservice::EnterGameResponese* response,
        ::google::protobuf::Closure* done)override;

    void OnLsLeaveGame(::google::protobuf::RpcController* controller,
        const controllerservice::LsLeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnLsDisconnect(::google::protobuf::RpcController* controller,
        const controllerservice::LsDisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGsPlayerService(::google::protobuf::RpcController* controller,
        const ::NodeServiceMessageRequest* request,
        ::NodeServiceMessageResponse* response,
        ::google::protobuf::Closure* done)override;

    void AddCrossServerScene(::google::protobuf::RpcController* controller,
        const controllerservice::AddCrossServerSceneRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void EnterGsSucceed(::google::protobuf::RpcController* controller,
        const controllerservice::EnterGsSucceedRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};