#include "team.h"

#include "src/game_logic/tips_id.h"
#include "src/game_logic/teams/team_event.h"
#include "src/game_logic/game_registry.h"

Team::Team(const CreateTeamP& param,
    const TeamsP& p)
    : teamid_(p.teamid_),
        teams_entity_id_(p.teams_entity_id_),
        leader_id_(param.leader_id_),
        emp_(p.emp_),
        teams_registry_(p.teams_registry_)
{
    for (auto& it : param.members)
    {
        members_.emplace_back(it);
    }
    auto& ms = playerid_team_map();
    for (auto& it : members_)
    {
        ms.emplace(it, teamid_);
        emp_->emit<JoinTeamEvent>(teamid_, it);
    }
}

Guid Team::first_applicant() const
{
    if (applicants_.empty())
    {
        return kInvalidGuid;
    }
    return *applicants_.begin();
}

uint32_t Team::CheckLimt(Guid  guid)
{
    return kRetOK;
}

uint32_t Team::JoinTeam(Guid  guid)
{
    if (HasTeam(guid))
    {
        return kRetTeamMemberInTeam;
    }
    if (IsFull())
    {
        return kRetTeamMembersFull;
    }
    DelApplicant(guid);
    members_.emplace_back(guid);
    playerid_team_map().emplace(guid, teamid_);
    emp_->emit<JoinTeamEvent>(teamid_, guid);
    return kRetOK;
}

uint32_t Team::LeaveTeam(Guid guid)
{
    if (!IsMember(guid))
    {
        return kRetTeamMemberNotInTeam;
    }
    bool leader_leave = IsLeader(guid);
    emp_->emit<BeforeLeaveTeamEvent>(teamid_, guid);
    auto it = std::find(members_.begin(), members_.end(), guid);
    members_.erase(it);//HasMember(guid) already check
    if (!members_.empty() && leader_leave)
    {
        OnAppointLeader(*members_.begin());
    }        
    playerid_team_map().erase(guid);
    emp_->emit<AfterLeaveTeamEvent>(teamid_, guid);     
    return kRetOK;
}

uint32_t Team::KickMember(Guid current_leader, Guid  kick_guid)
{
    if (leader_id_ != current_leader)
    {
        return kRetTeamKickNotLeader;
    }
    if (leader_id_ == kick_guid)
    {
        return kRetTeamKickSelf;
    }
    if (current_leader == kick_guid)
    {
        return kRetTeamKickSelf;
    }
    RET_CHECK_RET(LeaveTeam(kick_guid));
    return kRetOK;
}

uint32_t Team::AppointLeader(Guid current_leader, Guid new_leader)
{
    if (leader_id_ == new_leader)
    {
        return kRetTeamAppointSelf;
    }
    if (!IsMember(new_leader))
    {
        return kRetTeamHasNotTeamId;
    }
    if (leader_id_ != current_leader)
    {
        return kRetTeamAppointSelf;
    }
    OnAppointLeader(new_leader);
    return kRetOK;
}

void Team::OnAppointLeader(Guid guid)
{
    auto old_guid = leader_id_;
    leader_id_ = guid;
    emp_->emit<AppointLeaderEvent>(teamid_, old_guid, leader_id_);
}

uint32_t Team::DissMiss(Guid current_leader_id)
{
    if (leader_id() != current_leader_id)
    {
        return kRetTeamDismissNotLeader;
    }
    auto& ms = playerid_team_map();
    for (auto& it : members_)
    {
        emp_->emit<DissmisTeamEvent>(teamid_, it);
        ms.erase(it);               
    }
    return kRetOK;
}

void Team::ClearApplyList()
{
    applicants_.clear();      
}

uint32_t Team::ApplyToTeam(Guid guid)
{
	if (HasTeam(guid))
	{
		return kRetTeamMemberInTeam;
	}
	if (IsFull())
	{
		return kRetTeamMembersFull;
	}
	RET_CHECK_RET(CheckLimt(guid));
	if (applicants_.size() >= kMaxApplicantSize)
	{
		applicants_.erase(applicants_.begin());
	}
	applicants_.emplace_back(guid);
	return kRetOK;
}


uint32_t Team::DelApplicant(Guid applicant_id)
{
    auto it = std::find(applicants_.begin(), applicants_.end(), applicant_id);
    if (it != applicants_.end())
    {
        applicants_.erase(it);
    }
    return kRetOK;
}


