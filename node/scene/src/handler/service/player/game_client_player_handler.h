#pragma once

#include "proto/game/game_client_player.pb.h"

#include "service/player_service.h"

#include "macros/return_define.h"

class GameClientPlayerCommonServiceHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;


    static void SendTipToClient(entt::entity player,
        const ::TipInfoMessage* request,
        ::Empty* response);
    static void KickPlayer(entt::entity player,
        const ::GameKickPlayerRequest* request,
        ::Empty* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
            SendTipToClient(player,
                static_cast<const ::TipInfoMessage*>(request),
                static_cast<::Empty*>(response));
            break;
        case 1:
            KickPlayer(player,
                static_cast<const ::GameKickPlayerRequest*>(request),
                static_cast<::Empty*>(response));
            break;
        default:
            break;
        }
    }

};
