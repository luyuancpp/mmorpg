#pragma once
#include "logic/client_player/player_common.pb.h"
#include "player_service.h"
#include "macros/return_define.h"
class PlayerClientCommonServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void SendTipToClient(entt::entity player,
		const ::TipInfoMessage* request,
		::Empty* response);

	static void KickPlayer(entt::entity player,
		const ::TipInfoMessage* request,
		::Empty* response);

	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		entt::entity player,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response)override 
		{
		switch(method->index())
		{
		case 0:
			SendTipToClient(player,
			::google::protobuf::internal::DownCast<const TipInfoMessage*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 1:
			KickPlayer(player,
			::google::protobuf::internal::DownCast<const TipInfoMessage*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		default:
		break;
		}
	}

};
