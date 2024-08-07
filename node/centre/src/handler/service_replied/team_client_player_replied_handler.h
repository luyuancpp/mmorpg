#pragma once
#include "logic/client_player/team_client_player.pb.h"
#include "player_service_replied.h"
class ClientPlayerTeamServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
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
			nullptr,
			::google::protobuf::internal::DownCast<TeamInfoS2CResponse*>(response));
		break;
		default:
		break;
		}
	}

};
