#pragma once
#include "proto/scene/game_scene.pb.h"


class SceneSceneHandler : public ::SceneScene
{
public:


	void Test(::google::protobuf::RpcController* controller,
		const ::GameSceneTest* request,
		::Empty* response,
		::google::protobuf::Closure* done) override;


};