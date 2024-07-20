#pragma once
#include "logic/client_player/team_client_player.pb.h"
#include "player_service.h"
class ClientPlayerTeamServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void TeamInfoNotify(entt::entity player,
		const ::TeamInfoS2CRequest* request,
		::TeamInfoS2CResponse* response);

 void CallMethod(const ::google::protobuf::MethodDescriptor* method,
   entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override 
 		{
        switch(method->index())
		{
		case 0:
			TeamInfoNotify(player,
			::google::protobuf::internal::DownCast<const TeamInfoS2CRequest*>( request),
			::google::protobuf::internal::DownCast<TeamInfoS2CResponse*>(response));
		break;
		default:
		break;
		}
	}

};
