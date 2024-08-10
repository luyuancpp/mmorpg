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

	static void NotifyAbilityUsed(entt::entity player,
		const ::AbilityUsedS2C* request,
		::Empty* response);

	static void NotifyAbilityInterrupted(entt::entity player,
		const ::AbilityInterruptedS2C* request,
		::Empty* response);

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
		case 1:
			NotifyAbilityUsed(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 2:
			NotifyAbilityInterrupted(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		default:
		break;
		}
	}

};
