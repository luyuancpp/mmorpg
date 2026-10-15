#pragma once

#include <type_traits>
#include <utility>

#include "proto/scene/s2s_player_scene.pb.h"

#include "rpc/player_service_interface.h"

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
        ::EnterSceneS2CResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
			{
            static_assert(std::is_same_v<decltype(std::declval<::google::protobuf::Empty&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            EnterScene(player,
                static_cast<const ::GsEnterSceneRequest*>(request),
                static_cast<::google::protobuf::Empty*>(response));
			}
            break;
        case 1:
			{
            static_assert(std::is_same_v<decltype(std::declval<::google::protobuf::Empty&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            LeaveScene(player,
                static_cast<const ::GsLeaveSceneRequest*>(request),
                static_cast<::google::protobuf::Empty*>(response));
			}
            break;
        case 2:
			{
            static_assert(std::is_same_v<decltype(std::declval<::EnterSceneS2CResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            EnterSceneS2C(player,
                static_cast<const ::EnterSceneS2CRequest*>(request),
                static_cast<::EnterSceneS2CResponse*>(response));
			}
            break;
        default:
            break;
        }
    }

};
