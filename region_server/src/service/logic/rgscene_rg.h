#pragma once
#include "src/pb/pbc/logic_proto/scene_rg.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include <unordered_map>

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"
///<<< END WRITING YOUR CODE
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class RgServiceImpl : public regionservcie::RgService{
public:
///<<< BEGIN WRITING YOUR CODE
	using PlayerListMap = std::unordered_map<Guid, entt::entity>;
private:
	PlayerListMap  players_;
///<<< END WRITING YOUR CODE
public:
    void StartCrossGs(::google::protobuf::RpcController* controller,
        const regionservcie::StartCrossGsRequest* request,
        regionservcie::StartCrossGsResponse* response,
        ::google::protobuf::Closure* done)override;

    void StartMs(::google::protobuf::RpcController* controller,
        const regionservcie::StartMsRequest* request,
        regionservcie::StartMsResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterCrossMainScene(::google::protobuf::RpcController* controller,
        const regionservcie::EnterCrossMainSceneRequest* request,
        regionservcie::EnterCrossMainSceneResponese* response,
        ::google::protobuf::Closure* done)override;

    void EnterCrossMainSceneWeightRoundRobin(::google::protobuf::RpcController* controller,
        const regionservcie::EnterCrossMainSceneWeightRoundRobinRequest* request,
        regionservcie::EnterCrossRoomSceneSceneWeightRoundRobinResponse* response,
        ::google::protobuf::Closure* done)override;

};