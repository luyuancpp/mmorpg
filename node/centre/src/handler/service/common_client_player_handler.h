#pragma once
#include "proto/logic/client_player/common_client_player.pb.h"
#include "player_service.h"
class ClientPlayerCommonServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void PushTipsS2C(entt::entity player,
		const ::TipMessage* request,
		::TipMessage* response);

 void CallMethod(const ::google::protobuf::MethodDescriptor* method,
   entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override 
 		{
        switch(method->index())
		{
		case 0:
			PushTipsS2C(player,
			::google::protobuf::internal::DownCast<const TipMessage*>( request),
			::google::protobuf::internal::DownCast<TipMessage*>(response));
		break;
		default:
		break;
		}
	}

};
