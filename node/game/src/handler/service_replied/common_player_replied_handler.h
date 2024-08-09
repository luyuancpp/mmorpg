#pragma once
#include "logic/client_player/common_player.pb.h"
#include "player_service_replied.h"
class ClientPlayerCommonServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
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
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 1:
			KickPlayer(player,
			nullptr,
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		default:
		break;
		}
	}

};
