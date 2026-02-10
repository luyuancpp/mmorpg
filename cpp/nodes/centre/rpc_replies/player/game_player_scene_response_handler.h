
#pragma once
#include "proto/room/game_player_scene.pb.h"

#include "rpc/player_rpc_response_handler.h"


class SceneScenePlayerReply : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;


    static void EnterScene(entt::entity player,
        const ::GsEnterSceneRequest* request,
        ::google::protobuf::Empty* response);
    static void LeaveScene(entt::entity player,
        const ::GsLeaveSceneRequest* request,
        ::google::protobuf::Empty* response);
    static void EnterSceneS2C(entt::entity player,
        const ::EnterSceneS2CRequest* request,
        ::EnterScenerS2CResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
            EnterScene(player,
                nullptr,
                static_cast<::google::protobuf::Empty*>(response));
            break;
        case 1:
            LeaveScene(player,
                nullptr,
                static_cast<::google::protobuf::Empty*>(response));
            break;
        case 2:
            EnterSceneS2C(player,
                nullptr,
                static_cast<::EnterScenerS2CResponse*>(response));
            break;
        default:
            break;
        }
    }

};
