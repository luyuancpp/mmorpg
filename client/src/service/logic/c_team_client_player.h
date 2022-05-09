#pragma once
#include <sol/sol.hpp>
#include "player_service.h"
#include "src/game_logic/game_registry.h"
extern thread_local sol::state g_lua;
#include "logic_proto/team_client_player.pb.h"
class ClientPlayerTeamServiceService : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            g_lua["TeamInfoNotifyProcess"](
            ::google::protobuf::internal::DownCast<const ::TeamInfoS2CRequest*>( request),
            ::google::protobuf::internal::DownCast<::TeamInfoS2CResponse*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
