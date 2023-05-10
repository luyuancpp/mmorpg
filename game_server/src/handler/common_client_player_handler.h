#pragma once
#include "client_player_proto/common_client_player.pb.h"
#include "player_service.h"
class ClientPlayerCommonServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	void PushTipsS2C(entt::entity player,
		const ::TipsS2C* request,
		::TipsS2C* response);

 void CallMethod(const ::google::protobuf::MethodDescriptor* method,
   entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override 
 		{
        switch(method->index())
		{
		case 0:
			PushTipsS2C(player,
			::google::protobuf::internal::DownCast<const TipsS2C*>( request),
			::google::protobuf::internal::DownCast<TipsS2C*>(response));
		break;
		default:
		break;
		}
	}

};
