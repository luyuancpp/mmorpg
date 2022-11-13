#pragma once
#include "deploy_service.pb.h"
class DeployServiceImpl : public deploy::DeployService{
public:
public:
    void ServerInfo(::google::protobuf::RpcController* controller,
        const deploy::ServerInfoRequest* request,
        deploy::ServerInfoResponse* response,
        ::google::protobuf::Closure* done)override;

    void StartGS(::google::protobuf::RpcController* controller,
        const deploy::StartGSRequest* request,
        deploy::StartGSResponse* response,
        ::google::protobuf::Closure* done)override;

    void StartLobbyServer(::google::protobuf::RpcController* controller,
        const deploy::LobbyServerRequest* request,
        deploy::LobbyServerResponse* response,
        ::google::protobuf::Closure* done)override;

    void AcquireLobbyServerInfo(::google::protobuf::RpcController* controller,
        const deploy::LobbyServerRequest* request,
        deploy::LobbyServerResponse* response,
        ::google::protobuf::Closure* done)override;

    void AcquireLobbyInfo(::google::protobuf::RpcController* controller,
        const deploy::LobbyServerRequest* request,
        deploy::LobbyInfoResponse* response,
        ::google::protobuf::Closure* done)override;

    void LoginNodeInfo(::google::protobuf::RpcController* controller,
        const deploy::GroupLignRequest* request,
        deploy::GruoupLoginNodeResponse* response,
        ::google::protobuf::Closure* done)override;

    void SceneSqueueNodeId(::google::protobuf::RpcController* controller,
        const deploy::SceneSqueueRequest* request,
        deploy::SceneSqueueResponese* response,
        ::google::protobuf::Closure* done)override;

};