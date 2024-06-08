#pragma once
#include "logic_proto/centre_scene.pb.h"
class CentreSceneServiceHandler : public ::CentreSceneService
{
public:
	void UpdateCrossMainSceneInfo(::google::protobuf::RpcController* controller,
		const ::UpdateCrossMainSceneInfoRequest* request,
		::UpdateCrossMainSceneInfoResponse* response,
		 ::google::protobuf::Closure* done)override;

};

