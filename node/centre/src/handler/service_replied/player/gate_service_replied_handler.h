
#pragma once
#include "proto/gate/gate_service.pb.h"

#include "service/player_service_replied.h"


class GateServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;


    static void PlayerEnterGameNode(entt::entity player,
        const ::RegisterGameNodeSessionRequest* request,
        ::RegisterGameNodeSessionResponse* response);
    static void SendMessageToPlayer(entt::entity player,
        const ::NodeRouteMessageRequest* request,
        ::Empty* response);
    static void KickSessionByCentre(entt::entity player,
        const ::KickSessionRequest* request,
        ::Empty* response);
    static void RouteNodeMessage(entt::entity player,
        const ::RouteMessageRequest* request,
        ::RouteMessageResponse* response);
    static void RoutePlayerMessage(entt::entity player,
        const ::RoutePlayerMessageRequest* request,
        ::RoutePlayerMessageResponse* response);
    static void BroadcastToPlayers(entt::entity player,
        const ::BroadcastToPlayersRequest* request,
        ::Empty* response);
    static void RegisterNodeSession(entt::entity player,
        const ::RegisterNodeSessionRequest* request,
        ::RegisterNodeSessionResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
            PlayerEnterGameNode(player,
                nullptr,
                static_cast<::RegisterGameNodeSessionResponse*>(response));
            break;
        case 1:
            SendMessageToPlayer(player,
                nullptr,
                static_cast<::Empty*>(response));
            break;
        case 2:
            KickSessionByCentre(player,
                nullptr,
                static_cast<::Empty*>(response));
            break;
        case 3:
            RouteNodeMessage(player,
                nullptr,
                static_cast<::RouteMessageResponse*>(response));
            break;
        case 4:
            RoutePlayerMessage(player,
                nullptr,
                static_cast<::RoutePlayerMessageResponse*>(response));
            break;
        case 5:
            BroadcastToPlayers(player,
                nullptr,
                static_cast<::Empty*>(response));
            break;
        case 6:
            RegisterNodeSession(player,
                nullptr,
                static_cast<::RegisterNodeSessionResponse*>(response));
            break;
        default:
            break;
        }
    }

};
