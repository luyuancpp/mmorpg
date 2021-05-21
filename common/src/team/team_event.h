#ifndef COMMON_SRC_TEAM_Team_Event
#define COMMON_SRC_TEAM_Team_Event

#include "src/common_type/common_type.h"

namespace common
{
struct TeamEventStructBase
{
    TeamEventStructBase(GameGuid team_id, GameGuid player_id)
        : team_id_(team_id),
        player_id_(player_id)
    {

    }
    GameGuid team_id_{ kEmptyGameGuid };
    GameGuid player_id_{ kEmptyGameGuid };
};

struct TeamEventStructTeamIdBase
{
    TeamEventStructTeamIdBase(GameGuid team_id)
        : team_id_(team_id)
    {
    }
    GameGuid team_id_{ kEmptyGameGuid };
};

struct TeamEventStructDismissTeamOnTeamMemberEmpty : public TeamEventStructTeamIdBase { using TeamEventStructTeamIdBase::TeamEventStructTeamIdBase; };

struct TeamEventStructCreateTeamJoinTeam : public TeamEventStructBase { using TeamEventStructBase::TeamEventStructBase; };
struct TeamEventStructJoinTeam : public TeamEventStructBase { using TeamEventStructBase::TeamEventStructBase; };
struct TeamEventStructLeaderDismissTeam : public TeamEventStructBase { using TeamEventStructBase::TeamEventStructBase; };
struct TeamEventStructLeaveTeam : public TeamEventStructBase { using TeamEventStructBase::TeamEventStructBase; };

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

#endif // !COMMON_SRC_TEAM_Team_Event