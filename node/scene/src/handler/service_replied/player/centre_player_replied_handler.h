
#pragma once
#include "proto/logic/server_player/centre_player.pb.h"

#include "service/player_service_replied.h"


class CentrePlayerServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;


    static void Test(entt::entity player,
        const ::google::protobuf::Empty* request,
        ::google::protobuf::Empty* response);

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
                static_cast<::google::protobuf::Empty*>(response));
            break;
        default:
            break;
        }
    }

};
