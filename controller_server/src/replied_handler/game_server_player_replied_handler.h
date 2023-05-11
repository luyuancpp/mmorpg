#pragma once
#include "server_player_proto/game_server_player.pb.h"
#include "player_service_replied.h"
class GameLoginPlayerServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	using PlayerServiceReplied::PlayerServiceReplied;
	static void UpdateSessionController2Gs(entt::entity player,
		const ::UpdateSessionController2GsRequest* request,
		::google::protobuf::Empty* response);

	static void Controller2GsLogin(entt::entity player,
		const ::Controller2GsLoginRequest* request,
		::google::protobuf::Empty* response);

	static void Controller2GsEnterGateSucceed(entt::entity player,
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
			UpdateSessionController2Gs(player,
			nullptr,
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 1:
			Controller2GsLogin(player,
			nullptr,
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		case 2:
			Controller2GsEnterGateSucceed(player,
			nullptr,
			::google::protobuf::internal::DownCast<google::protobuf::Empty*>(response));
		break;
		default:
		break;
		}
	}

};
