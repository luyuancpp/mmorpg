#pragma once
#include "src/pb/pbc/logic_proto/lobby_scene.pb.h"
class LobbyServiceImpl : public ::LobbyService{
public:
public:
    void StartCrossGs(::google::protobuf::RpcController* controller,
        const ::StartCrossGsRequest* request,
        ::StartCrossGsResponse* response,
        ::google::protobuf::Closure* done)override;

    void StartControllerNode(::google::protobuf::RpcController* controller,
        const ::StartControllerRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void EnterCrossMainScene(::google::protobuf::RpcController* controller,
        const ::EnterCrossMainSceneRequest* request,
        ::EnterCrossMainSceneResponese* response,
        ::google::protobuf::Closure* done)override;

    void EnterCrossMainSceneWeightRoundRobin(::google::protobuf::RpcController* controller,
        const ::EnterCrossMainSceneWeightRoundRobinRequest* request,
        ::EnterCrossRoomSceneSceneWeightRoundRobinResponse* response,
        ::google::protobuf::Closure* done)override;

    void LeaveCrossMainScene(::google::protobuf::RpcController* controller,
        const ::LeaveCrossMainSceneRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void GameConnectToController(::google::protobuf::RpcController* controller,
        const ::GameConnectToControllerRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};