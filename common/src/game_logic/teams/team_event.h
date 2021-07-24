#ifndef COMMON_SRC_TEAM_TEAM_EVENT_H
#define COMMON_SRC_TEAM_TEAM_EVENT_H

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

namespace common
{
struct TeamESBase
{
    TeamESBase(entt::entity team_id, GameGuid player_id)
        : team_id_(team_id),
        player_id_(player_id)
    {

    }
    entt::entity team_id_{ kEmptyGameGuid };
    GameGuid player_id_{ kEmptyGameGuid };
};

struct TeamESTeamIdBase
{
    TeamESTeamIdBase(entt::entity team_id)
        : team_id_(team_id)
    {
    }
    entt::entity team_id_{ kEmptyGameGuid };
};

struct TeamESJoinTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamESLeaderDismissTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamESLeaveTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamESLeaderLeaveTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamESClearApplyList : public TeamESTeamIdBase { using TeamESTeamIdBase::TeamESTeamIdBase; };

struct TeamESAppointLeader 
{
    TeamESAppointLeader(entt::entity team_id, GameGuid current_player_id, GameGuid new_player_id)
        : team_id_(team_id),
          current_leader_player_id_(current_player_id),
          new_leader_player_id_(new_player_id)
    {
    }
    entt::entity team_id_{ kEmptyGameGuid };
    GameGuid current_leader_player_id_{ kEmptyGameGuid };
    GameGuid new_leader_player_id_{ kEmptyGameGuid };
};
}

#endif // !COMMON_SRC_TEAM_TEAM_EVENT_H