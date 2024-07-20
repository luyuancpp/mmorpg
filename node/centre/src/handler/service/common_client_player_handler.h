#pragma once
#include "client_player_proto/common_client_player.pb.h"
#include "player_service.h"
class ClientPlayerCommonServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void PushTipsS2C(entt::entity player,
		const ::TipS2C* request,
		::TipS2C* response);

 void CallMethod(const ::google::protobuf::MethodDescriptor* method,
   entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override 
 		{
        switch(method->index())
		{
		case 0:
			PushTipsS2C(player,
			::google::protobuf::internal::DownCast<const TipS2C*>( request),
			::google::protobuf::internal::DownCast<TipS2C*>(response));
		break;
		default:
		break;
		}
	}

};
