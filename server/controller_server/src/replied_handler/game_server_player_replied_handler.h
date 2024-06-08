#pragma once
#include "server_player_proto/game_server_player.pb.h"
#include "player_service_replied.h"
class GamePlayerServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
	static void Controller2GsLogin(entt::entity player,
		const ::Centre2GsLoginRequest* request,
		::google::protobuf::Empty* response);

	static void OnControllerEnterGateSucceed(entt::entity player,
		const ::Controller2GsEnterGateSucceedRequest* request,
		::google::protobuf::Empty* response);

 void CallMethod(const ::google::protobuf::MethodDescriptor* method,
   entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override 
 		{
        switch(method->index())
		{
		case 0:
			Controller2GsLogin(player,
			nullptr,
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 1:
			OnControllerEnterGateSucceed(player,
			nullptr,
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		default:
		break;
		}
	}

};
