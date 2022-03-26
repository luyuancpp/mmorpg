#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_PLAYER_TEAM_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_PLAYER_TEAM_H_
#include "player_service.h"
#include "logic_proto/player_team.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace playerteam{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerC2GsTeamServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const playerteam::EnterSceneRequest* request,
        playerteam::EnterSceneResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const playerteam::EnterSceneRequest*>( request),
            ::google::protobuf::internal::DownCast<playerteam::EnterSceneResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace playerteam
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_PLAYER_TEAM_H_
