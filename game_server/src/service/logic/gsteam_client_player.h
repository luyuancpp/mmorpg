#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_TEAM_CLIENT_PLAYER_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_TEAM_CLIENT_PLAYER_H_
#include "player_service.h"
#include "logic_proto/team_client_player.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace clientplayer{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerTeamServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const clientplayer::TeamTestRequest* request,
        clientplayer::TeamTestResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const clientplayer::TeamTestRequest*>( request),
            ::google::protobuf::internal::DownCast<clientplayer::TeamTestResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace clientplayer
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_TEAM_CLIENT_PLAYER_H_
