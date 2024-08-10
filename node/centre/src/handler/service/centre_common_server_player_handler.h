#pragma once
#include "logic/server_player/centre_common_server_player.pb.h"
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
			::google::protobuf::internal::DownCast<const google::protobuf::Empty*>(request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		default:
		break;
		}
	}

};
