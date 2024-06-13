#pragma once
#include "logic_proto/centre_scene.pb.h"
class CentreSceneServiceHandler : public ::CentreSceneService
{
public:
	void RegisterScene(::google::protobuf::RpcController* controller,
		const ::RegisterSceneRequest* request,
		::RegisterSceneResponse* response,
		 ::google::protobuf::Closure* done)override;

	void UnRegisterScene(::google::protobuf::RpcController* controller,
		const ::UnRegisterSceneRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

};

