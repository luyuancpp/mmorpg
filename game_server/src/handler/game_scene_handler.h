#pragma once
#include "logic_proto/game_scene.pb.h"
class GameSceneServiceHandler : public ::GameSceneService
{
public:
	void Test(::google::protobuf::RpcController* controller,
		const ::GameSceneTest* request,
		::google::protobuf::Empty* response,
		 ::google::protobuf::Closure* done)override;

};

