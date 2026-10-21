#pragma once

#include <type_traits>
#include <utility>

#include "proto/scene/player_skill.pb.h"

#include "rpc/player_service_interface.h"

#include "macros/return_define.h"

class SceneSkillClientPlayerHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;

    static void ReleaseSkill(entt::entity player,
        const ::ReleaseSkillRequest* request,
        ::ReleaseSkillResponse* response);
    static void NotifySkillUsed(entt::entity player,
        const ::SkillUsedS2C* request,
        ::Empty* response);
    static void NotifySkillInterrupted(entt::entity player,
        const ::SkillInterruptedS2C* request,
        ::Empty* response);
    static void ListSkills(entt::entity player,
        const ::ListSkillsRequest* request,
        ::ListSkillsResponse* response);
    static void UpgradeSkill(entt::entity player,
        const ::UpgradeSkillRequest* request,
        ::UpgradeSkillResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
			{
            static_assert(std::is_same_v<decltype(std::declval<::ReleaseSkillResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            ReleaseSkill(player,
                static_cast<const ::ReleaseSkillRequest*>(request),
                static_cast<::ReleaseSkillResponse*>(response));
			}
            break;
        case 1:
			{
            NotifySkillUsed(player,
                static_cast<const ::SkillUsedS2C*>(request),
                static_cast<::Empty*>(response));
			}
            break;
        case 2:
			{
            NotifySkillInterrupted(player,
                static_cast<const ::SkillInterruptedS2C*>(request),
                static_cast<::Empty*>(response));
			}
            break;
        case 3:
			{
            static_assert(std::is_same_v<decltype(std::declval<::ListSkillsResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            ListSkills(player,
                static_cast<const ::ListSkillsRequest*>(request),
                static_cast<::ListSkillsResponse*>(response));
			}
            break;
        case 4:
			{
            static_assert(std::is_same_v<decltype(std::declval<::UpgradeSkillResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            UpgradeSkill(player,
                static_cast<const ::UpgradeSkillRequest*>(request),
                static_cast<::UpgradeSkillResponse*>(response));
			}
            break;
        default:
            break;
        }
    }

};
