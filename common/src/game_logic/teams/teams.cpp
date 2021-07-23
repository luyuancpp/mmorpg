#include "teams.h"

#include "src/return_code/return_notice_code.h"

namespace common
{
#define GetTeamPtrReturnError \
    auto e = entt::to_entity(team_id);\
    if (!teams_.valid(e))\
    {\
        return RET_TEAM_HAS_NOT_TEAM_ID;\
    }\
    auto& team = teams_.get<Team>(e);\

#define GetTeamEntityReturnError \
    auto e = team_id;\
    if (!teams_.valid(e))\
    {\
        return RET_TEAM_HAS_NOT_TEAM_ID;\
    }\
    auto& team = teams_.get<Team>(e);\

#define GetTeamReturn(ret) \
    auto e = entt::to_entity(team_id);\
    if (!teams_.valid(e))\
    {\
        return ret;\
    }\
    auto& team = teams_.get<Team>(e);\


    TeamList::TeamList()
        : emp_(EventManager::New())
    {
        emp_->subscribe<TeamESCreateTeamJoinTeam>(*this);
        emp_->subscribe<TeamESJoinTeam>(*this);
        emp_->subscribe<TeamESLeaderDismissTeam>(*this);     
        emp_->subscribe<TeamESLeaveTeam>(*this);  

    }

    std::size_t TeamList::member_size(GameGuid team_id)
    {
        GetTeamReturn(0);
        return team.member_size();
    }

    std::size_t TeamList::applicant_size_by_player_id(GameGuid player_id) const
    {
        auto team_id = GetTeamId(player_id);
        return applicant_size_by_team_id(team_id);
    }

    std::size_t TeamList::applicant_size_by_team_id(GameGuid team_id) const
    {
        GetTeamReturn(0);
        return team.applicant_size();
    }

    const TeamMember& TeamList::team_member(GameGuid player_id)const
    {
        auto team_id = GetTeamEntityId(player_id);
        static TeamMember m;
        if (!teams_.valid(team_id))
        {
            return m;
        }
        auto& team = teams_.get<Team>(team_id);;
        return team.team_member(player_id);
    }

    GameGuid TeamList::GetTeamId(GameGuid player_id)const
    {
        auto it = player_team_map_.find(player_id);
        if (it == player_team_map_.end())
        {
            return kEmptyGameGuid;
        }
        return entt::to_integral(it->second);
    }

    entt::entity TeamList::GetTeamEntityId(GameGuid player_id) const
    {
        auto it = player_team_map_.find(player_id);
        if (it == player_team_map_.end())
        {
            return entt::null;
        }
        return it->second;
    }

    common::GameGuid TeamList::leader_id_by_teamid(GameGuid team_id) const
    {
        GetTeamReturn(kEmptyGameGuid);
        return team.leader_id();
    }

    common::GameGuid TeamList::leader_id_by_player_id(GameGuid player_id) const
    {
        auto team_id = GetTeamId(player_id);
        return leader_id_by_teamid(team_id);
    }
    
    common::GameGuid TeamList::first_applicant_id(GameGuid team_id) const
    {
        GetTeamReturn(0);
        return team.first_applicant_id();
    }

    bool TeamList::IsTeamsMax()const
    {
        return team_size() >= kMaxTeamSize;
    }

    bool TeamList::IsTeamFull(GameGuid team_id)
    {
        GetTeamReturn(false);
        return team.IsFull();
    }

    bool TeamList::PlayerInTeam(GameGuid team_id, GameGuid player_id)
    {
        GetTeamReturn(false);
        return team.InTeam(player_id);
    }

    bool TeamList::FindTeamId(GameGuid player_id)
    {
        return GetTeamId(player_id) != kEmptyGameGuid;
    }

    bool TeamList::HasApplicant(GameGuid team_id, GameGuid player_id) const
    {
        GetTeamReturn(false);
        return team.HasApplicant(player_id);
    }

    bool TeamList::IsLeader(GameGuid team_id, GameGuid player_id)
    {
        GetTeamReturn(false);
        return team.IsLeader(player_id);
    }

    bool TeamList::IsLeader(GameGuid player_id)
    {
        return IsLeader(GetTeamId(player_id), player_id);
    }

    bool TeamList::TestApplicantValueEqual(GameGuid team_id)const
    {
        GetTeamReturn(false);
        return team.TestApplicantValueEqual();
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

        auto e = teams_.create();
        auto team = teams_.emplace<Team>(e,  e, emp_, param, &teams_);
        team.OnCreate();
        last_team_id_ = entt::to_integral(e);
        return RET_OK;
    }

    ReturnValue TeamList::JoinTeam(GameGuid team_id, TeamMember& mem)
    {
        if (PlayerInTeam(mem.player_id()))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        GetTeamPtrReturnError;
        return team.JoinTeam(mem);
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
        auto team_id = GetTeamEntityId(player_id);
        GetTeamEntityReturnError;
        RET_CHECK_RET(team.LeaveTeam(player_id));
        if (team.empty())
        {
            EraseTeam(team.to_entityid());
        }
        return RET_OK;
    }

    ReturnValue TeamList::KickMember(GameGuid team_id, GameGuid current_leader_id, GameGuid  kick_player_id)
    {
        GetTeamPtrReturnError;
        RET_CHECK_RET(team.KickMember(current_leader_id, kick_player_id));
        return RET_OK;
    }

    ReturnValue TeamList::DissMissTeam(GameGuid team_id, GameGuid current_leader_id)
    {
        GetTeamPtrReturnError;
        if (team.leader_id() != current_leader_id)
        {
            return RET_TEAM_DISMISS_NOT_LEADER;
        }
        for (auto& it : team.members())
        {
            emp_->emit<TeamESLeaderDismissTeam>(e, it.second.player_id());
        }
        EraseTeam(e);
        return RET_OK;
    }

    ReturnValue TeamList::DissMissTeamNoLeader(GameGuid team_id)
    {
        GetTeamPtrReturnError;
        return DissMissTeam(team_id, team.leader_id());
    }

    ReturnValue TeamList::AppointLeader(GameGuid team_id, GameGuid current_leader_id, GameGuid  new_leader_player_id)
    {
        GetTeamPtrReturnError;
        return team.AppointLeader(current_leader_id, new_leader_player_id);
    }

    ReturnValue TeamList::ApplyForTeam(GameGuid team_id, const TeamMember& m)
    {
        GetTeamPtrReturnError;
        if (PlayerInTeam(m.player_id()))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        return team.ApplyForTeam(m);
    }

    ReturnValue TeamList::RemoveApplicant(GameGuid team_id, GameGuid nApplyplayer_id)
    {
        GetTeamPtrReturnError;
        return team.RemoveApplicant(nApplyplayer_id);
    }

    ReturnValue TeamList::AgreeApplicant(GameGuid team_id, GameGuid applicant_id)
    {
        GetTeamPtrReturnError;
        if (PlayerInTeam(applicant_id))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        return team.AgreeApplicant(applicant_id);
    }

    void TeamList::ClearApplyList(GameGuid team_id)
    {
        auto e = entt::to_entity(team_id);
        if (!teams_.valid(e))
        {
            return;
        }
        auto& team = teams_.get<Team>(e);
        team.ClearApplyList();
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
            RET_CHECK_RET(team.JoinTeam(it.second));
        }
        return RET_OK;
    }

    void TeamList::EraseTeam(entt::entity team_id)
    {
        teams_.destroy(team_id);
    }

}//namespace common

