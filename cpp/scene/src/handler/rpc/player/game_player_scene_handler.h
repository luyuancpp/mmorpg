#pragma once

#include "proto/scene/game_player_scene.pb.h"

#include "rpc/player_service.h"

#include "macros/return_define.h"

class SceneScenePlayerHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;


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
                static_cast<const ::GsEnterSceneRequest*>(request),
                static_cast<::google::protobuf::Empty*>(response));
            break;
        case 1:
            LeaveScene(player,
                static_cast<const ::GsLeaveSceneRequest*>(request),
                static_cast<::google::protobuf::Empty*>(response));
            break;
        case 2:
            EnterSceneS2C(player,
                static_cast<const ::EnterSceneS2CRequest*>(request),
                static_cast<::EnterScenerS2CResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::EnterScenerS2CResponse*>(response));
            break;
        default:
            break;
        }
    }

};
