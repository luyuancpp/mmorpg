#pragma once
#include "logic/client_player/player_ability.pb.h"
#include "player_service.h"
#include "macros/return_define.h"
class PlayerAbilityServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
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
			::google::protobuf::internal::DownCast<const UseAbilityRequest*>(request),
			::google::protobuf::internal::DownCast<UseAbilityResponse*>(response));
			HANDLE_ERROR_MESSAGE(::google::protobuf::internal::DownCast<UseAbilityResponse*>(response));
		break;
		default:
		break;
		}
	}

};
