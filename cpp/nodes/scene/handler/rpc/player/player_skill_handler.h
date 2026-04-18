#pragma once

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

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
			{
            ReleaseSkill(player,
                static_cast<const ::ReleaseSkillRequest*>(request),
                static_cast<::ReleaseSkillResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::ReleaseSkillResponse*>(response));
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
            ListSkills(player,
                static_cast<const ::ListSkillsRequest*>(request),
                static_cast<::ListSkillsResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::ListSkillsResponse*>(response));
			}
            break;
        default:
            break;
        }
    }

};
