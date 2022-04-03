#ifndef MASTER_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_TEAM_SERVER_PLAYER_H_
#define MASTER_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_TEAM_SERVER_PLAYER_H_
#include "../player_service.h"
#include "logic_proto/team_server_player.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace serverplayer{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class PlayerPlayerTeamServiceImpl : public game::PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(common::EntityPtr& entity,
        const serverplayer::TeamTestRequest* request,
        serverplayer::TeamTestResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const serverplayer::TeamTestRequest*>( request),
            ::google::protobuf::internal::DownCast<serverplayer::TeamTestResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace serverplayer
#endif//MASTER_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_TEAM_SERVER_PLAYER_H_
