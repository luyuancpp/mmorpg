#ifndef _LOGIC_PROTO_PLAYER_TEAM_MS_H_
#define _LOGIC_PROTO_PLAYER_TEAM_MS_H_
#include "player_service.h"
#include "logic_proto/player/team_ms.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace msplayerservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerPlayerTeamServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const msplayerservice::TeamTestRequest* request,
        msplayerservice::TeamTestResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const msplayerservice::TeamTestRequest*>( request),
            ::google::protobuf::internal::DownCast<msplayerservice::TeamTestResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace msplayerservice
#endif//_LOGIC_PROTO_PLAYER_TEAM_MS_H_
