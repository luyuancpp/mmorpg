#pragma once
#include "logic/client_player/player_state_attribute_sync.pb.h"
#include "player_service.h"
#include "macros/return_define.h"
class EntitySyncServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void SyncBaseAttribute(entt::entity player,
		const ::BaseAttributeDeltaS2C* request,
		::Empty* response);

	static void SyncAttribute(entt::entity player,
		const ::AttributeDelta* request,
		::Empty* response);

	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		entt::entity player,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response)override 
		{
		switch(method->index())
		{
		case 0:
			SyncBaseAttribute(player,
			::google::protobuf::internal::DownCast<const BaseAttributeDeltaS2C*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 1:
			SyncAttribute(player,
			::google::protobuf::internal::DownCast<const AttributeDelta*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		default:
		break;
		}
	}

};
