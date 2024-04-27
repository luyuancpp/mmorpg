#pragma once
#include "common_proto/deploy_service.pb.h"
class DeployServiceImpl : public ::DeployService{
public:
public:
    void ServerInfo(::google::protobuf::RpcController* controller,
        const ::ServerInfoRequest* request,
        ::ServerInfoResponse* response,
        ::google::protobuf::Closure* done)override;

    void StartGS(::google::protobuf::RpcController* controller,
        const ::StartGSRequest* request,
        ::StartGSResponse* response,
        ::google::protobuf::Closure* done)override;

    void StartLobbyServer(::google::protobuf::RpcController* controller,
        const ::LobbyServerRequest* request,
        ::LobbyServerResponse* response,
        ::google::protobuf::Closure* done)override;

    void AcquireLobbyServerInfo(::google::protobuf::RpcController* controller,
        const ::LobbyServerRequest* request,
        ::LobbyServerResponse* response,
        ::google::protobuf::Closure* done)override;

    void AcquireLobbyInfo(::google::protobuf::RpcController* controller,
        const ::LobbyServerRequest* request,
        ::LobbyInfoResponse* response,
        ::google::protobuf::Closure* done)override;

    void LoginNodeInfo(::google::protobuf::RpcController* controller,
        const ::GroupLignRequest* request,
        ::GruoupLoginNodeResponse* response,
        ::google::protobuf::Closure* done)override;

    void SceneSequenceNodeId(::google::protobuf::RpcController* controller,
        const ::SceneSqueueRequest* request,
        ::SceneSqueueResponese* response,
        ::google::protobuf::Closure* done)override;

};