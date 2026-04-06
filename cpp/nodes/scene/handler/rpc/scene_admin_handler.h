#pragma once
#include "proto/scene/scene_admin.pb.h"

class SceneSceneHandler : public ::SceneScene
{
public:

	void Test(::google::protobuf::RpcController* controller,  const ::GameSceneTest* request, ::Empty* response, ::google::protobuf::Closure* done) override;

};