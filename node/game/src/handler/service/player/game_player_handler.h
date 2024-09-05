#pragma once
#include "logic/server_player/game_player.pb.h"
#include "player_service.h"
#include "macros/return_define.h"
class GamePlayerServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void Centre2GsLogin(entt::entity player,
		const ::Centre2GsLoginRequest* request,
		::google::protobuf::Empty* response);

	static void ExitGame(entt::entity player,
		const ::GameNodeExitGameRequest* request,
		::google::protobuf::Empty* response);

	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		entt::entity player,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response)override 
		{
		switch(method->index())
		{
		case 0:
			Centre2GsLogin(player,
			::google::protobuf::internal::DownCast<const Centre2GsLoginRequest*>(request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 1:
			ExitGame(player,
			::google::protobuf::internal::DownCast<const GameNodeExitGameRequest*>(request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		default:
		break;
		}
	}

};
