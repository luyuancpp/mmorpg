#pragma once
#include "server_player_proto/team_server_player.pb.h"
#include "player_service_replied.h"
class ServerPlayerTeamServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
	PlayerServiceReplied::PlayerServiceReplied;
	void EnterScene(entt::entity player,
		const ::TeamTestRequest* request,
		::TeamTestResponse* response);

 void CallMethod(const ::google::protobuf::MethodDescriptor* method,
   entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override 
  {
        switch(method->index()) {
		case 0:
			EnterScene(player,
			nullptr,
			::google::protobuf::internal::DownCast<TeamTestResponse*>(response));
		break;
		default:
			GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
		break;
		}
	}

};
