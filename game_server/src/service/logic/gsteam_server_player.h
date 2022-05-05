#pragma once
#include "player_service.h"
#include "logic_proto/team_server_player.pb.h"
class ServerPlayerTeamServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void EnterScene(EntityPtr& entity,
        const ::TeamTestRequest* request,
        ::TeamTestResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    EntityPtr& entity,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            EnterScene(entity,
            ::google::protobuf::internal::DownCast<const ::TeamTestRequest*>( request),
            ::google::protobuf::internal::DownCast<::TeamTestResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
