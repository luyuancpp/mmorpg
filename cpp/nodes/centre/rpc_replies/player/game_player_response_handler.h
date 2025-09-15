
#pragma once
#include "proto/service/rpc/scene/game_player.pb.h"

#include "rpc/player_rpc_response_handler.h"


class ScenePlayerReply : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;


    static void Centre2GsLogin(entt::entity player,
        const ::Centre2GsLoginRequest* request,
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
            Centre2GsLogin(player,
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
