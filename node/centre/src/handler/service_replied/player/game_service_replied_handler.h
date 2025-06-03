
#pragma once
#include "proto/scene/game_service.pb.h"

#include "service/player_service_replied.h"


class GameServiceRepliedHandler : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;


    static void PlayerEnterGameNode(entt::entity player,
        const ::PlayerEnterGameNodeRequest* request,
        ::Empty* response);
    static void SendMessageToPlayer(entt::entity player,
        const ::NodeRouteMessageRequest* request,
        ::NodeRouteMessageResponse* response);
    static void ClientSendMessageToPlayer(entt::entity player,
        const ::ClientSendMessageToPlayerRequest* request,
        ::ClientSendMessageToPlayerResponse* response);
    static void CentreSendToPlayerViaGameNode(entt::entity player,
        const ::NodeRouteMessageRequest* request,
        ::Empty* response);
    static void InvokePlayerService(entt::entity player,
        const ::NodeRouteMessageRequest* request,
        ::NodeRouteMessageResponse* response);
    static void RouteNodeStringMsg(entt::entity player,
        const ::RouteMessageRequest* request,
        ::RouteMessageResponse* response);
    static void RoutePlayerStringMsg(entt::entity player,
        const ::RoutePlayerMessageRequest* request,
        ::RoutePlayerMessageResponse* response);
    static void UpdateSessionDetail(entt::entity player,
        const ::RegisterPlayerSessionRequest* request,
        ::Empty* response);
    static void EnterScene(entt::entity player,
        const ::Centre2GsEnterSceneRequest* request,
        ::Empty* response);
    static void CreateScene(entt::entity player,
        const ::CreateSceneRequest* request,
        ::CreateSceneResponse* response);
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
                static_cast<::Empty*>(response));
            break;
        case 1:
            SendMessageToPlayer(player,
                nullptr,
                static_cast<::NodeRouteMessageResponse*>(response));
            break;
        case 2:
            ClientSendMessageToPlayer(player,
                nullptr,
                static_cast<::ClientSendMessageToPlayerResponse*>(response));
            break;
        case 3:
            CentreSendToPlayerViaGameNode(player,
                nullptr,
                static_cast<::Empty*>(response));
            break;
        case 4:
            InvokePlayerService(player,
                nullptr,
                static_cast<::NodeRouteMessageResponse*>(response));
            break;
        case 5:
            RouteNodeStringMsg(player,
                nullptr,
                static_cast<::RouteMessageResponse*>(response));
            break;
        case 6:
            RoutePlayerStringMsg(player,
                nullptr,
                static_cast<::RoutePlayerMessageResponse*>(response));
            break;
        case 7:
            UpdateSessionDetail(player,
                nullptr,
                static_cast<::Empty*>(response));
            break;
        case 8:
            EnterScene(player,
                nullptr,
                static_cast<::Empty*>(response));
            break;
        case 9:
            CreateScene(player,
                nullptr,
                static_cast<::CreateSceneResponse*>(response));
            break;
        case 10:
            RegisterNodeSession(player,
                nullptr,
                static_cast<::RegisterNodeSessionResponse*>(response));
            break;
        default:
            break;
        }
    }

};
