#pragma once
#include <sol/sol.hpp>
#include "player_service.h"
#include "src/game_logic/thread_local/game_registry.h"
extern thread_local sol::state g_lua;
#include "logic_proto/common_client_player.pb.h"
class ClientPlayerCommonServiceService : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            g_lua["PushTipsS2CProcess"](
            ::google::protobuf::internal::DownCast<const ::TipsS2C*>( request),
            ::google::protobuf::internal::DownCast<::TipsS2C*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
