#pragma once
#include "logic_proto/controller_scene.pb.h"
class CentreSceneServiceHandler : public ::ControllerSceneService
{
public:
	void UpdateCrossMainSceneInfo(::google::protobuf::RpcController* controller,
		const ::UpdateCrossMainSceneInfoRequest* request,
		::UpdateCrossMainSceneInfoResponse* response,
		 ::google::protobuf::Closure* done)override;

};

