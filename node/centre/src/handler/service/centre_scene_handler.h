#pragma once
#include "proto/centre/centre_scene.pb.h"


class CentreSceneHandler : public ::CentreScene
{
public:


	void RegisterScene(::google::protobuf::RpcController* controller,
		const ::RegisterSceneRequest* request,
		::RegisterSceneResponse* response,
		::google::protobuf::Closure* done) override;



	void UnRegisterScene(::google::protobuf::RpcController* controller,
		const ::UnRegisterSceneRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done) override;


};