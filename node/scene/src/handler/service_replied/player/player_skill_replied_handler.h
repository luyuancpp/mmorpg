
#pragma once
#include "proto/scene/player_skill.pb.h"

#include "service/player_service_replied.h"


class SceneSkillClientPlayerRepliedHandler : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;


    static void ReleaseSkill(entt::entity player,
        const ::ReleaseSkillSkillRequest* request,
        ::ReleaseSkillSkillResponse* response);
    static void NotifySkillUsed(entt::entity player,
        const ::SkillUsedS2C* request,
        ::Empty* response);
    static void NotifySkillInterrupted(entt::entity player,
        const ::SkillInterruptedS2C* request,
        ::Empty* response);
    static void GetSkillList(entt::entity player,
        const ::GetSkillListRequest* request,
        ::GetSkillListResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
            ReleaseSkill(player,
                nullptr,
                static_cast<::ReleaseSkillSkillResponse*>(response));
            break;
        case 1:
            NotifySkillUsed(player,
                nullptr,
                static_cast<::Empty*>(response));
            break;
        case 2:
            NotifySkillInterrupted(player,
                nullptr,
                static_cast<::Empty*>(response));
            break;
        case 3:
            GetSkillList(player,
                nullptr,
                static_cast<::GetSkillListResponse*>(response));
            break;
        default:
            break;
        }
    }

};
