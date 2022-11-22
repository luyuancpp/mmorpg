#pragma once
#include "src/pb/pbc/logic_proto/scene.pb.h"
class ServerSceneServiceImpl : public ::ServerSceneService{
public:
public:
    void UpdateCrossMainSceneInfo(::google::protobuf::RpcController* controller,
        const ::UpdateCrossMainSceneInfoRequest* request,
        ::UpdateCrossMainSceneInfoResponse* response,
        ::google::protobuf::Closure* done)override;

};