#include "teams.h"

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

    TeamList::TeamList()
        : emp_(EventManager::New())
    {
        emp_->subscribe<TeamESCreateTeamJoinTeam>(*this);
        emp_->subscribe<TeamESJoinTeam>(*this);
        emp_->subscribe<TeamESLeaderDismissTeam>(*this);     
        emp_->subscribe<TeamESLeaveTeam>(*this);  
        emp_->subscribe<TeamESDismissTeamOnTeamMemberEmpty>(*this);
    }

    std::size_t TeamList::member_size(GameGuid team_id)
    {
        auto it = teams_.find(team_id);
        if (it == teams_.end())
        {
            return 0;
        }
        return it->second->member_size();
    }

    std::size_t TeamList::applicant_size_by_player_id(GameGuid player_id) const
    {
        auto team_id = GetTeamId(player_id);
        return applicant_size_by_team_id(team_id);
    }

    std::size_t TeamList::applicant_size_by_team_id(GameGuid team_id) const
    {
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

    void TeamList::receive(const TeamESJoinTeam& es)
    {
        OnJoinTeam(es.player_id_, es.team_id_);
    }

    void TeamList::receive(const TeamESCreateTeamJoinTeam& es)
    {
        OnJoinTeam(es.player_id_, es.team_id_);
    }

    void TeamList::receive(const TeamESLeaderDismissTeam& es)
    {
        OnPlayerLeaveTeam(es.player_id_);
    }

    void TeamList::receive(const TeamESLeaveTeam& es)
    {
        OnPlayerLeaveTeam(es.player_id_);
    }

    void TeamList::receive(const TeamESDismissTeamOnTeamMemberEmpty& es)
    {
        EraseTeam(es.team_id_);
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
        RET_CHECK_RET(CheckMemberInTeam(param.members));

        auto team_id = snow_flake_.Generate();
        TeamPtr p_team = std::make_shared<Team>(team_id, emp_, param);
        std::pair<TeamMap::iterator, bool> p = teams_.emplace(team_id, std::move(p_team));
        if (p.second)
        {
            last_team_id_ = team_id;
        }
        return RET_OK;
    }

    ReturnValue TeamList::JoinTeam(GameGuid team_id, TeamMember& mem)
    {
        if (PlayerInTeam(mem.player_id()))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        GetTeamPtrReturnError;
        return p_team->JoinTeam(mem);
    }

    ReturnValue TeamList::CheckMemberInTeam(const Members& member_list)
    {
        for (auto& it : member_list)
        {
            if (PlayerInTeam(it.second.player_id()))
            {
                return RET_TEAM_MEMBER_IN_TEAM;
            }
        }
        return RET_OK;
    }

    void TeamList::OnPlayerLeaveTeam(GameGuid player_id)
    {
        player_team_map_.erase(player_id);
    }

    ReturnValue TeamList::LeaveTeam(GameGuid player_id)
    {
        auto team_id = GetTeamId(player_id);
        GetTeamPtrReturnError;
        RET_CHECK_RET(p_team->LeaveTeam(player_id));
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
        for (auto& it : p_team->members())
        {
            emp_->emit<TeamESLeaderDismissTeam>(team_id, it.second.player_id());
        }
        EraseTeam(team_id);
        return RET_OK;
    }

    ReturnValue TeamList::DissMissTeamNoLeader(GameGuid team_id)
    {
        GetTeamPtrReturnError;
        return DissMissTeam(team_id, p_team->leader_id());
    }

    ReturnValue TeamList::AppointLeader(GameGuid team_id, GameGuid current_leader_id, GameGuid  new_leader_player_id)
    {
        GetTeamPtrReturnError;
        return p_team->AppointLeader(current_leader_id, new_leader_player_id);
    }

    ReturnValue TeamList::ApplyForTeam(GameGuid team_id, const TeamMember& m)
    {
        GetTeamPtrReturnError;
        if (PlayerInTeam(m.player_id()))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        return p_team->ApplyForTeam(m);
    }

    ReturnValue TeamList::RemoveApplicant(GameGuid team_id, GameGuid nApplyplayer_id)
    {
        GetTeamPtrReturnError;
        return p_team->RemoveApplicant(nApplyplayer_id);
    }

    ReturnValue TeamList::AgreeApplicant(GameGuid team_id, GameGuid applicant_id)
    {
        GetTeamPtrReturnError;
        if (PlayerInTeam(applicant_id))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        return p_team->AgreeApplicant(applicant_id);
    }

    void TeamList::ClearApplyList(GameGuid team_id)
    {
        auto it = teams_.find(team_id);
        if (it != teams_.end())
        {
            it->second->ClearApplyList();
        }
    }

    ReturnValue TeamList::JoinTeam(const Members& member_list, GameGuid  team_id)
    {
        GetTeamPtrReturnError;
        RET_CHECK_RET(CheckMemberInTeam(member_list));
        for (auto& it : member_list)
        {
            if (PlayerInTeam(it.second.player_id()))
            {
                return RET_TEAM_MEMBER_IN_TEAM;
            }
            RET_CHECK_RET(p_team->JoinTeam(it.second));
        }
        return RET_OK;
    }

    void TeamList::EraseTeam(GameGuid team_id)
    {
        teams_.erase(team_id);
    }

}//namespace common

