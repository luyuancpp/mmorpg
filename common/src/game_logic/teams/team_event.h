#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

struct TeamEvent
{
    TeamEvent(entt::entity team_id, Guid guid)
        : teamid_(team_id),
        guid_(guid)
    {

    }
    entt::entity teamid_{ kInvalidGuid };
    Guid guid_{ kInvalidGuid };
};

struct TeamESTeamIdBase
{
    TeamESTeamIdBase(entt::entity team_id)
        : teamid_(team_id)
    {
    }
    entt::entity teamid_{ kInvalidGuid };
};

struct JoinTeamEvent : public TeamEvent { using TeamEvent::TeamEvent; };
struct BeforeLeaveTeamEvent : public TeamEvent { using TeamEvent::TeamEvent; };
struct AfterLeaveTeamEvent : public TeamEvent { using TeamEvent::TeamEvent; };
struct DissmisTeamEvent : public TeamEvent { using TeamEvent::TeamEvent; };
struct BeforeLeaderLeaveTeamEvent : public TeamEvent { using TeamEvent::TeamEvent; };
struct ClearApplyListEvent : public TeamESTeamIdBase { using TeamESTeamIdBase::TeamESTeamIdBase; };

struct AppointLeaderEvent 
{
    AppointLeaderEvent(entt::entity team_id, Guid current_guid, Guid new_guid)
        : teamid_(team_id),
          current_leader_guid_(current_guid),
          new_leader_guid_(new_guid)
    {
    }
    entt::entity teamid_{ kInvalidGuid };
    Guid current_leader_guid_{ kInvalidGuid };
    Guid new_leader_guid_{ kInvalidGuid };
};
