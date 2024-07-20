#pragma once
#include "logic_proto/server_player_proto/game_server_player.pb.h"
#include "player_service.h"
class GamePlayerServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void Centre2GsLogin(entt::entity player,
		const ::Centre2GsLoginRequest* request,
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
			::google::protobuf::internal::DownCast<const Centre2GsLoginRequest*>( request),
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		default:
		break;
		}
	}

};
