#pragma once
#include "client_player_proto/common_client_player.pb.h"
#include "player_service_replied.h"
class ClientPlayerCommonServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
	static void PushTipsS2C(entt::entity player,
		const ::TipsS2C* request,
		::TipsS2C* response);

	static void BeKick(entt::entity player,
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
			nullptr,
			::google::protobuf::internal::DownCast<TipsS2C*>(response));
		break;
		case 1:
			BeKick(player,
			nullptr,
			::google::protobuf::internal::DownCast<TipsS2C*>(response));
		break;
		default:
		break;
		}
	}

};
