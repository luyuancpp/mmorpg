#pragma once

#include "proto/scene/game_player.pb.h"

#include "rpc/player_service_interface.h"

#include "macros/return_define.h"

class ScenePlayerHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;

    static void GateLoginNotify(entt::entity player,
        const ::GateLoginNotifyRequest* request,
        ::google::protobuf::Empty* response);
    static void ExitGame(entt::entity player,
        const ::GameNodeExitGameRequest* request,
        ::google::protobuf::Empty* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
			{
            GateLoginNotify(player,
                static_cast<const ::GateLoginNotifyRequest*>(request),
                static_cast<::google::protobuf::Empty*>(response));
			}
            break;
        case 1:
			{
            ExitGame(player,
                static_cast<const ::GameNodeExitGameRequest*>(request),
                static_cast<::google::protobuf::Empty*>(response));
			}
            break;
        default:
            break;
        }
    }

};
