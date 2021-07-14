#ifndef COMMON_SRC_TEAM_TEAM_EVENT_H
#define COMMON_SRC_TEAM_TEAM_EVENT_H

#include "src/common_type/common_type.h"

namespace common
{
struct TeamESBase
{
    TeamESBase(GameGuid team_id, GameGuid player_id)
        : team_id_(team_id),
        player_id_(player_id)
    {

    }
    GameGuid team_id_{ kEmptyGameGuid };
    GameGuid player_id_{ kEmptyGameGuid };
};

struct TeamESTeamIdBase
{
    TeamESTeamIdBase(GameGuid team_id)
        : team_id_(team_id)
    {
    }
    GameGuid team_id_{ kEmptyGameGuid };
};

struct TeamEventStructDismissTeamOnTeamMemberEmpty : public TeamESTeamIdBase { using TeamESTeamIdBase::TeamESTeamIdBase; };

struct TeamEventStructCreateTeamJoinTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamEventStructJoinTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamEventStructLeaderDismissTeam : public TeamESBase { using TeamESBase::TeamESBase; };
struct TeamEventStructLeaveTeam : public TeamESBase { using TeamESBase::TeamESBase; };

struct TeamEventStructAppointLeader 
{
    TeamEventStructAppointLeader(GameGuid team_id, GameGuid current_player_id, GameGuid new_player_id)
        : team_id_(team_id),
          current_leader_player_id_(current_player_id),
          new_leader_player_id_(new_player_id)
    {
    }
    GameGuid team_id_{ kEmptyGameGuid };
    GameGuid current_leader_player_id_{ kEmptyGameGuid };
    GameGuid new_leader_player_id_{ kEmptyGameGuid };
};
}

#endif // !COMMON_SRC_TEAM_TEAM_EVENT_H