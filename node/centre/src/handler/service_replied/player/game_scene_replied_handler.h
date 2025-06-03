
#pragma once
#include "proto/scene/game_scene.pb.h"

#include "service/player_service_replied.h"


class GameSceneServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;


    static void Test(entt::entity player,
        const ::GameSceneTest* request,
        ::Empty* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
            Test(player,
                nullptr,
                static_cast<::Empty*>(response));
            break;
        default:
            break;
        }
    }

};
