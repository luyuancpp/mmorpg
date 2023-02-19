#pragma once
#include <sol/sol.hpp>
#include "player_service.h"
#include "src/game_logic/thread_local/thread_local_storage_lua.h"
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
            tls_lua_state["EnterSceneC2SProcess"](
            ::google::protobuf::internal::DownCast<const ::EnterSeceneC2SRequest*>( request),
            ::google::protobuf::internal::DownCast<::EnterSeceneC2SResponse*>(response));
        break;
        case 1:
            tls_lua_state["PushEnterSceneS2CProcess"](
            ::google::protobuf::internal::DownCast<const ::EnterSeceneS2C*>( request),
            ::google::protobuf::internal::DownCast<::EnterSeceneS2C*>(response));
        break;
        case 2:
            tls_lua_state["PushSceneInfoS2CProcess"](
            ::google::protobuf::internal::DownCast<const ::SceneInfoS2C*>( request),
            ::google::protobuf::internal::DownCast<::SceneInfoS2C*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
    }
};
