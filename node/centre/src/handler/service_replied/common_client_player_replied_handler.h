#pragma once
#include "logic/client_player/common_client_player.pb.h"
#include "player_service_replied.h"
class ClientPlayerCommonServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
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
			nullptr,
			::google::protobuf::internal::DownCast<TipS2C*>(response));
		break;
		case 1:
			BeKick(player,
			nullptr,
			::google::protobuf::internal::DownCast<TipS2C*>(response));
		break;
		default:
		break;
		}
	}

};
