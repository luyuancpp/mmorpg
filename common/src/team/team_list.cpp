#include "team_list.h"

#include "src/return_code/return_notice_code.h"

namespace common
{
#define GetTeamPtrReturnError \
    auto it = teams_.find(team_id);\
    if (it == teams_.end())\
    {\
        return common::RET_TEAM_HAS_NOT_TEAM_ID;\
    }\
    auto p_team = it->second;

#define GetTeamPtrReturnFalse \
    auto it = teams_.find(team_id);\
    if (it == teams_.end())\
    {\
        return false;\
    }\
    auto p_team = it->second;

    std::size_t TeamList::member_size(GameGuid team_id)
    {
        TeamMap::iterator it = teams_.find(team_id);
        if (it == teams_.end())
        {
            return 0;
        }
        return it->second->member_size();
    }

    const TeamMember& TeamList::team_member(GameGuid player_id)
    {
        GameGuid teamId = GetTeamId(player_id);
        static TeamMember m;
        if (teamId == kEmptyTeamId)
        {
            return m;
        }
        TeamMap::iterator it = teams_.find(teamId);
        if (it == teams_.end())
        {
            return m;
        }
        return it->second->team_member(player_id);
    }

    ReturnValue TeamList::CreateTeam(const CreateTeamParam& param)
    {
        if (IsTeamListFull())
        {
            return RET_TEAM_TEAM_LIST_MAX;
        }
        if (PlayerInTeam(param.leader_id_))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        auto team_id = snow_flake_.Generate();
        TeamPtr p_team(
            new Team(param));
        std::pair<TeamMap::iterator, bool> p = teams_.emplace(team_id, std::move(p_team));
        return RET_OK;
    }

    ReturnValue TeamList::JoinTeam(GameGuid team_id, TeamMember& mem)
    {
        GetTeamPtrReturnError;
        return p_team->JoinTeam(mem);
    }

    ReturnValue TeamList::LeaveTeam(GameGuid team_id, GameGuid player_id)
    {
        GetTeamPtrReturnError;
        RET_CHECK_RET(p_team->LeaveTeam(player_id));
        EraseTeam(team_id);
        return RET_OK;
    }

    ReturnValue TeamList::KickMember(GameGuid team_id, GameGuid current_leader_id, GameGuid  kick_player_id)
    {
        GetTeamPtrReturnError;
        RET_CHECK_RET(p_team->KickMember(current_leader_id, kick_player_id));
        return RET_OK;
    }

    ReturnValue TeamList::DissMissTeam(GameGuid team_id, GameGuid current_leader_id)
    {
        GetTeamPtrReturnError;
        if (p_team->leader_id() != current_leader_id)
        {
            return RET_TEAM_DISMISS_NOT_LEADER;
        }
        EraseTeam(team_id);
        return RET_OK;
    }

    ReturnValue TeamList::DissMissTeamNoLeader(GameGuid team_id)
    {
        GetTeamPtrReturnError;
        return DissMissTeam(team_id, p_team->leader_id());
    }

    ReturnValue TeamList::AppointLeader(GameGuid team_id, GameGuid current_leader_id, GameGuid  nNewLeaderplayer_id)
    {
        GetTeamPtrReturnError;
        return p_team->AppointLeader(current_leader_id, nNewLeaderplayer_id);
    }

    ReturnValue TeamList::Apply(GameGuid team_id, const TeamMember& m)
    {
        GetTeamPtrReturnError;
        return p_team->Apply(m);
    }

    ReturnValue TeamList::RemoveApplicant(GameGuid team_id, GameGuid nApplyplayer_id)
    {
        GetTeamPtrReturnError;
        return p_team->RemoveApplicant(nApplyplayer_id);
    }

    ReturnValue TeamList::AgreeApplicant(GameGuid team_id, GameGuid nApplyplayer_id)
    {
        GetTeamPtrReturnError;
        return p_team->AgreeApplicant(nApplyplayer_id);
    }

    void TeamList::ClearApplyList(GameGuid team_id)
    {
        TeamMap::iterator it = teams_.find(team_id);
        if (it != teams_.end())
        {
            it->second->ClearApplyList();
        }
    }

    bool TeamList::IsTeamListFull()const
    {
        return teams_.size() >= kMaxTeamSize;
    }

    bool TeamList::IsTeamFull(GameGuid team_id)
    {
        GetTeamPtrReturnFalse;
        return p_team->IsFull();
    }

    bool TeamList::InTeam(GameGuid team_id, GameGuid nplayer_id)
    {
        GetTeamPtrReturnFalse;
        return p_team->InTeam(nplayer_id);
    }

    bool TeamList::FindTeamId(GameGuid nplayer_id)
    {
        return GetTeamId(nplayer_id) != kEmptyTeamId;
    }

    bool TeamList::IsLeader(GameGuid team_id, GameGuid nplayer_id)
    {
        GetTeamPtrReturnFalse;
        return p_team->IsLeader(nplayer_id);
    }

    bool TeamList::IsLeader(GameGuid nplayer_id)
    {
        PlayerIdTeamIdMap::iterator it = player_team_map_.find(nplayer_id);
        if (it == player_team_map_.end())
        {
            return false;
        }
        return IsLeader(it->second, nplayer_id);
    }
  
    GameGuid TeamList::GetTeamId(GameGuid nplayer_id)
    {
        PlayerIdTeamIdMap::iterator it = player_team_map_.find(nplayer_id);
        if (it == player_team_map_.end())
        {
            return kEmptyTeamId;
        }
        return it->second;        
    }

    ReturnValue TeamList::JoinTeam(const Members& member_list, GameGuid  team_id)
    {
        GetTeamPtrReturnError;
        for (auto& it : member_list)
        {
            RET_CHECK_RET(p_team->JoinTeam(it.second));
        }
        return RET_OK;
    }

    void TeamList::EraseTeam(GameGuid team_id)
    {
        teams_.erase(team_id);
    }

}//namespace common

