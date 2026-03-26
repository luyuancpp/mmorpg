
#pragma once
#include "proto/scene/game_player.pb.h"

#include "rpc/player_rpc_response_handler.h"

class ScenePlayerReply : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;

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
            GateLoginNotify(player,
                nullptr,
                static_cast<::google::protobuf::Empty*>(response));
            break;
        case 1:
            ExitGame(player,
                nullptr,
                static_cast<::google::protobuf::Empty*>(response));
            break;
        default:
            break;
        }
    }

};
