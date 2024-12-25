#pragma once
#include "proto/logic/server/game_scene.pb.h"
class GameSceneServiceHandler : public ::GameSceneService
{
public:
	void Test(::google::protobuf::RpcController* controller,
		const ::GameSceneTest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


};

