#ifndef _LOGIC_PROTO_PLAYER_TEAM_GS_H_
#define _LOGIC_PROTO_PLAYER_TEAM_GS_H_
#include "player_service.h"
#include "logic_proto/player/team_gs.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace playerservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerPlayerTeamServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const playerservice::TeamTestRequest* request,
        playerservice::TeamTestResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const playerservice::TeamTestRequest*>( request),
            ::google::protobuf::internal::DownCast<playerservice::TeamTestResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace playerservice
#endif//_LOGIC_PROTO_PLAYER_TEAM_GS_H_
