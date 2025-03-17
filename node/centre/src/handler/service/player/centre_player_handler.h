#pragma once
#include "proto/logic/server_player/centre_player.pb.h"
#include "player_service.h"
#include "macros/return_define.h"
class CentrePlayerServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void Test(entt::entity player,
		const ::google::protobuf::Empty* request,
		::google::protobuf::Empty* response);

	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		entt::entity player,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response)override 
		{
		switch(method->index())
		{
		case 0:
			Test(player,
			static_cast<const google::protobuf::Empty*>(request),
			static_cast<google::protobuf::Empty*>(response));
		break;
		default:
		break;
		}
	}

};
