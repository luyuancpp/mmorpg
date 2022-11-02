#pragma once
#include "src/pb/pbc/logic_proto/lobby_scene.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include <unordered_map>

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"
///<<< END WRITING YOUR CODE
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class RgServiceImpl : public lobbyservcie::RgService{
public:
///<<< BEGIN WRITING YOUR CODE
	using PlayerListMap = std::unordered_map<Guid, entt::entity>;
private:
	PlayerListMap  players_;
///<<< END WRITING YOUR CODE
public:
    void StartCrossGs(::google::protobuf::RpcController* controller,
        const lobbyservcie::StartCrossGsRequest* request,
        lobbyservcie::StartCrossGsResponse* response,
        ::google::protobuf::Closure* done)override;

    void StartControllerNode(::google::protobuf::RpcController* controller,
        const lobbyservcie::StartControllerRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void EnterCrossMainScene(::google::protobuf::RpcController* controller,
        const lobbyservcie::EnterCrossMainSceneRequest* request,
        lobbyservcie::EnterCrossMainSceneResponese* response,
        ::google::protobuf::Closure* done)override;

    void EnterCrossMainSceneWeightRoundRobin(::google::protobuf::RpcController* controller,
        const lobbyservcie::EnterCrossMainSceneWeightRoundRobinRequest* request,
        lobbyservcie::EnterCrossRoomSceneSceneWeightRoundRobinResponse* response,
        ::google::protobuf::Closure* done)override;

    void LeaveCrossMainScene(::google::protobuf::RpcController* controller,
        const lobbyservcie::LeaveCrossMainSceneRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};