#ifndef GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_TEAM_CLIENT_PLAYER_H_
#define GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_TEAM_CLIENT_PLAYER_H_
#include "player_service.h"
#include "logic_proto/team_client_player.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class ClientPlayerTeamServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void TeamInfoNotify(EntityPtr& entity,
        const ::TeamInfoS2CRequest* request,
        ::TeamInfoS2CResponse* response);

        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            TeamInfoNotify(entity,
            ::google::protobuf::internal::DownCast<const ::TeamInfoS2CRequest*>( request),
            ::google::protobuf::internal::DownCast<::TeamInfoS2CResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
#endif//GAME_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_TEAM_CLIENT_PLAYER_H_
