#pragma once
#include "scene.pb.h"
class ServerSceneServiceHandler : public ::ServerSceneService
{
public:
	void UpdateCrossMainSceneInfo(::google::protobuf::RpcController* controller,
		const ::UpdateCrossMainSceneInfoRequest* request,
		::UpdateCrossMainSceneInfoResponse* response,
		 ::google::protobuf::Closure* done)override;

};

