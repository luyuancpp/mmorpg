#pragma once
#include "player_service.h"
#include "logic_proto/team_client_player.pb.h"
class ClientPlayerTeamServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    entt::entity player,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
