#pragma once

#include <type_traits>
#include <utility>

#include "proto/scene/player_lifecycle.pb.h"

#include "rpc/player_service_interface.h"

#include "macros/return_define.h"

class ScenePlayerHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;

    static void GateLoginNotify(entt::entity player,
        const ::GateLoginNotifyRequest* request,
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
			{
            static_assert(std::is_same_v<decltype(std::declval<::google::protobuf::Empty&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GateLoginNotify(player,
                static_cast<const ::GateLoginNotifyRequest*>(request),
                static_cast<::google::protobuf::Empty*>(response));
			}
            break;
        case 1:
			{
            static_assert(std::is_same_v<decltype(std::declval<::google::protobuf::Empty&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            ExitGame(player,
                static_cast<const ::GameNodeExitGameRequest*>(request),
                static_cast<::google::protobuf::Empty*>(response));
			}
            break;
        default:
            break;
        }
    }

};
