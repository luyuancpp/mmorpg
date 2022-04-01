#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_TEAM_GS_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_TEAM_GS_H_
#include "player_service.h"
#include "logic_proto/team_gs.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace gsplayerservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerPlayerTeamServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const gsplayerservice::TeamTestRequest* request,
        gsplayerservice::TeamTestResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const gsplayerservice::TeamTestRequest*>( request),
            ::google::protobuf::internal::DownCast<gsplayerservice::TeamTestResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace gsplayerservice
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_PROTO_TEAM_GS_H_
