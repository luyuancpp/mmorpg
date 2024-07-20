#pragma once
#include "logic/client_player/common_client_player.pb.h"
#include "player_service.h"
class ClientPlayerCommonServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void PushTipS2C(entt::entity player,
		const ::TipS2C* request,
		::TipS2C* response);

	static void BeKick(entt::entity player,
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
			PushTipS2C(player,
			::google::protobuf::internal::DownCast<const TipS2C*>( request),
			::google::protobuf::internal::DownCast<TipS2C*>(response));
		break;
		case 1:
			BeKick(player,
			::google::protobuf::internal::DownCast<const TipS2C*>( request),
			::google::protobuf::internal::DownCast<TipS2C*>(response));
		break;
		default:
		break;
		}
	}

};
