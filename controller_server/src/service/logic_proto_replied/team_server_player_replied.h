#pragma once
#include "player_service_replied.h"
#include "logic_proto/team_server_player.pb.h"
class ServerPlayerTeamServiceRepliedImpl : public PlayerServiceReplied {
public:
    using PlayerService::PlayerService;
public:
    void EnterScene(entt::entity player,
        ::TeamTestResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            EnterScene(player,
::TeamTestRequest*>( request),
            ::google::protobuf::internal::DownCast<::TeamTestResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
