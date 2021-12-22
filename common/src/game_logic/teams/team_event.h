#ifndef COMMON_SRC_TEAM_TEAM_EVENT_H
#define COMMON_SRC_TEAM_TEAM_EVENT_H

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

namespace common
{
struct TeamESBase
{
    TeamESBase(entt::entity team_id, Guid guid)
        : teamid_(team_id),
        guid_(guid)
    {

    }
    entt::entity teamid_{ kEmptyGuid };
    Guid guid_{ kEmptyGuid };
};

struct TeamESTeamIdBase
{
    TeamESTeamIdBase(entt::entity team_id)
        : teamid_(team_id)
    {
    }
    entt::entity teamid_{ kEmptyGuid };
};

struct TeamESJoinTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamESLeaderDismissTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamESLeaveTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamESLeaderLeaveTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamESClearApplyList : public TeamESTeamIdBase { using TeamESTeamIdBase::TeamESTeamIdBase; };

struct TeamESAppointLeader 
{
    TeamESAppointLeader(entt::entity team_id, Guid current_guid, Guid new_guid)
        : teamid_(team_id),
          current_leader_guid_(current_guid),
          new_leader_guid_(new_guid)
    {
    }
    entt::entity teamid_{ kEmptyGuid };
    Guid current_leader_guid_{ kEmptyGuid };
    Guid new_leader_guid_{ kEmptyGuid };
};
}

#endif // !COMMON_SRC_TEAM_TEAM_EVENT_H