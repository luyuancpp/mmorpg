#pragma once
#include "logic/client_player/player_ability.pb.h"
#include "player_service_replied.h"

class PlayerAbilityServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
	static void UseAbility(entt::entity player,
		const ::UseAbilityRequest* request,
		::UseAbilityResponse* response);

	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		entt::entity player,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response)override 
		{
		switch(method->index())
		{
		case 0:
			UseAbility(player,
			nullptr,
			::google::protobuf::internal::DownCast<UseAbilityResponse*>(response));
		break;
		default:
		break;
		}
	}

};
