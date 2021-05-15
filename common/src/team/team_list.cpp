#include "team_list.h"

#include "src/return_code/return_notice_code.h"

namespace common
{
#define GetTeamPtrReturnError \
    auto it = teams_.find(team_id);\
    if (it == teams_.end())\
    {\
        return RET_TEAM_HAS_NOT_TEAM_ID;\
    }\
    auto p_team = it->second;

#define GetTeamPtrReturn(ret) \
    auto it = teams_.find(team_id);\
    if (it == teams_.end())\
    {\
        return ret;\
    }\
    auto p_team = it->second;

    std::size_t TeamList::member_size(GameGuid team_id)
    {
        auto it = teams_.find(team_id);
        if (it == teams_.end())
        {
            return 0;
        }
        return it->second->member_size();
    }

    std::size_t TeamList::applicant_size(GameGuid player_id) const
    {
        auto team_id = GetTeamId(player_id);
        GetTeamPtrReturn(0);
        return p_team->applicant_size();
    }

    const TeamMember& TeamList::team_member(GameGuid player_id)const
    {
        GameGuid team_id = GetTeamId(player_id);
        static TeamMember m;
        if (team_id == kEmptyGameGuid)
        {
            return m;
        }
        auto it = teams_.find(team_id);
        if (it == teams_.end())
        {
            return m;
        }
        return it->second->team_member(player_id);
    }

    GameGuid TeamList::GetTeamId(GameGuid player_id)const
    {
        auto it = player_team_map_.find(player_id);
        if (it == player_team_map_.end())
        {
            return kEmptyGameGuid;
        }
        return it->second;
    }

    common::GameGuid TeamList::leader_id_by_teamid(GameGuid team_id) const
    {
        GetTeamPtrReturn(kEmptyGameGuid);
        return p_team->leader_id();
    }

    common::GameGuid TeamList::leader_id_by_player_id(GameGuid player_id) const
    {
        auto team_id = GetTeamId(player_id);
        return leader_id_by_teamid(team_id);
    }
    
    common::GameGuid TeamList::first_applicant_id(GameGuid team_id) const
    {
        GetTeamPtrReturn(0);
        return p_team->first_applicant_id();
    }

    ReturnValue TeamList::CreateTeam(const CreateTeamParam& param)
    {
        if (IsTeamsMax())
        {
            return RET_TEAM_TEAM_LIST_MAX;
        }
        if (PlayerInTeam(param.leader_id_))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        auto team_id = snow_flake_.Generate();
        TeamPtr p_team(
            new Team(team_id, param));
        std::pair<TeamMap::iterator, bool> p = teams_.emplace(team_id, std::move(p_team));
        if (p.second)
        {
            last_team_id_ = team_id;
        }
        return RET_OK;
    }

    ReturnValue TeamList::JoinTeam(GameGuid team_id, TeamMember& mem)
    {
        GetTeamPtrReturnError;
        return p_team->JoinTeam(mem);
    }

    ReturnValue TeamList::LeaveTeam(GameGuid player_id)
    {
        auto team_id = GetTeamId(player_id);
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
        auto it = teams_.find(team_id);
        if (it != teams_.end())
        {
            it->second->ClearApplyList();
        }
    }

    bool TeamList::IsTeamsMax()const
    {
        return teams_.size() >= kMaxTeamSize;
    }

    bool TeamList::IsTeamFull(GameGuid team_id)
    {
        GetTeamPtrReturn(false);
        return p_team->IsFull();
    }

    bool TeamList::PlayerInTeam(GameGuid team_id, GameGuid player_id)
    {
        GetTeamPtrReturn(false);
        return p_team->InTeam(player_id);
    }

    bool TeamList::FindTeamId(GameGuid player_id)
    {
        return GetTeamId(player_id) != kEmptyGameGuid;
    }

    bool TeamList::HasApplicant(GameGuid team_id, GameGuid player_id) const
    {
        GetTeamPtrReturn(false);
        return p_team->HasApplicant(player_id);
    }

    bool TeamList::IsLeader(GameGuid team_id, GameGuid player_id)
    {
        GetTeamPtrReturn(false);
        return p_team->IsLeader(player_id);
    }

    bool TeamList::IsLeader(GameGuid player_id)
    {
        return IsLeader(GetTeamId(player_id), player_id);
    }

    bool TeamList::TestApplicantValueEqual(GameGuid team_id)const
    {
        GetTeamPtrReturn(false);
        return p_team->TestApplicantValueEqual();
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

