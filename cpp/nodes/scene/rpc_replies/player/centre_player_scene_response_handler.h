
#pragma once
#include "proto/service/cpp/rpc/centre/centre_player_scene.pb.h"

#include "rpc/player_rpc_response_handler.h"


class CentrePlayerSceneReply : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;


    static void EnterScene(entt::entity player,
        const ::CentreEnterSceneRequest* request,
        ::google::protobuf::Empty* response);
    static void LeaveScene(entt::entity player,
        const ::CentreLeaveSceneRequest* request,
        ::google::protobuf::Empty* response);
    static void LeaveSceneAsyncSavePlayerComplete(entt::entity player,
        const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest* request,
        ::google::protobuf::Empty* response);
    static void SceneInfoC2S(entt::entity player,
        const ::CentreSceneInfoRequest* request,
        ::google::protobuf::Empty* response);

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
            LeaveSceneAsyncSavePlayerComplete(player,
                nullptr,
                static_cast<::google::protobuf::Empty*>(response));
            break;
        case 3:
            SceneInfoC2S(player,
                nullptr,
                static_cast<::google::protobuf::Empty*>(response));
            break;
        default:
            break;
        }
    }

};
