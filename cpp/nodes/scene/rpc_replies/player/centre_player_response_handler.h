
#pragma once
#include "proto/service/rpc/centre/centre_player.pb.h"

#include "rpc/player_rpc_response_handler.h"


class CentrePlayerUtilityReply : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;


    static void SendTipToClient(entt::entity player,
        const ::TipInfoMessage* request,
        ::Empty* response);
    static void KickPlayer(entt::entity player,
        const ::CentreKickPlayerRequest* request,
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
                nullptr,
                static_cast<::Empty*>(response));
            break;
        case 1:
            KickPlayer(player,
                nullptr,
                static_cast<::Empty*>(response));
            break;
        default:
            break;
        }
    }

};
