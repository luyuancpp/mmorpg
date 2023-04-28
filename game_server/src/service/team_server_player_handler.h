#pragma once
#include "team_server_player.pb.h"
#include "player_service.h"
class ServerPlayerTeamServiceHandler : public ::PlayerService
{
public:
	PlayerService::PlayerService;
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
			::google::protobuf::internal::DownCast<const TeamTestRequest*>( request),
			::google::protobuf::internal::DownCast<TeamTestResponse*>(response));
		break;
		default:
			GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
		break;
		}
	}
};

