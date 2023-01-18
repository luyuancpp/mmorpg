#pragma once
#include <sol/sol.hpp>
#include "player_service.h"
#include "src/game_logic/thread_local/game_registry.h"
extern thread_local sol::state g_lua;
#include "logic_proto/scene_client_player.pb.h"
class ClientPlayerSceneServiceService : public PlayerService {
public:
    using PlayerService::PlayerService;
public:
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response)override
    {
        switch(method->index()) {
        case 0:
            g_lua["EnterSceneC2SProcess"](
            ::google::protobuf::internal::DownCast<const ::EnterSeceneC2SRequest*>( request),
            ::google::protobuf::internal::DownCast<::EnterSeceneC2SResponse*>(response));
        break;
        case 1:
            g_lua["PushEnterSceneS2CProcess"](
            ::google::protobuf::internal::DownCast<const ::EnterSeceneS2C*>( request),
            ::google::protobuf::internal::DownCast<::EnterSeceneS2C*>(response));
        break;
        case 2:
            g_lua["PushSceneInfoS2CProcess"](
            ::google::protobuf::internal::DownCast<const ::SceneInfoS2C*>( request),
            ::google::protobuf::internal::DownCast<::SceneInfoS2C*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
