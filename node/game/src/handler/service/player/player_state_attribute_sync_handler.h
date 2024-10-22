#pragma once
#include "logic/client_player/player_state_attribute_sync.pb.h"
#include "player_service.h"
#include "macros/return_define.h"
class EntityStateSyncServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void SyncVelocity(entt::entity player,
		const ::SyncEntityVelocityS2C* request,
		::Empty* response);

	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		entt::entity player,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response)override 
		{
		switch(method->index())
		{
		case 0:
			SyncVelocity(player,
			::google::protobuf::internal::DownCast<const SyncEntityVelocityS2C*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		default:
		break;
		}
	}

};
