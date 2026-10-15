#pragma once

#include <type_traits>
#include <utility>

#include "proto/scene/player_mission.pb.h"

#include "rpc/player_service_interface.h"

#include "macros/return_define.h"

class SceneMissionClientPlayerHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;

    static void GetMissionList(entt::entity player,
        const ::GetMissionListRequest* request,
        ::GetMissionListResponse* response);
    static void AcceptMission(entt::entity player,
        const ::MissionActionRequest* request,
        ::GetMissionListResponse* response);
    static void ClaimMissionReward(entt::entity player,
        const ::MissionActionRequest* request,
        ::GetMissionListResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GetMissionListResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GetMissionList(player,
                static_cast<const ::GetMissionListRequest*>(request),
                static_cast<::GetMissionListResponse*>(response));
			}
            break;
        case 1:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GetMissionListResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            AcceptMission(player,
                static_cast<const ::MissionActionRequest*>(request),
                static_cast<::GetMissionListResponse*>(response));
			}
            break;
        case 2:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GetMissionListResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            ClaimMissionReward(player,
                static_cast<const ::MissionActionRequest*>(request),
                static_cast<::GetMissionListResponse*>(response));
			}
            break;
        default:
            break;
        }
    }

};
