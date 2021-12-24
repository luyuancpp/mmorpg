#include "team.h"

#include "src/return_code/error_code.h"
#include "src/game_logic/teams/team_event.h"
#include "src/game_logic/game_registry.h"

namespace common
{
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
            emp_->emit<TeamESJoinTeam>(teamid_, it);
        }
    }

    Guid Team::first_applicant() const
    {
        if (applicants_.empty())
        {
            return kEmptyGuid;
        }
        return *applicants_.begin();
    }

    uint32_t Team::CheckLimt(Guid  guid)
    {
        return RET_OK;
    }

    uint32_t Team::JoinTeam(Guid  guid)
    {
        if (HasTeam(guid))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        if (IsFull())
        {
            return RET_TEAM_MEMBERS_FULL;
        }
        DelApplicant(guid);
        members_.emplace_back(guid);
        playerid_team_map().emplace(guid, teamid_);
        emp_->emit<TeamESJoinTeam>(teamid_, guid);
        return RET_OK;
    }

    uint32_t Team::LeaveTeam(Guid guid)
    {
        if (!IsMember(guid))
        {
            return RET_TEAM_MEMBER_NOT_IN_TEAM;
        }
        bool leader_leave = IsLeader(guid);
        emp_->emit<EeventBeforeLeaveTeam>(teamid_, guid);
        auto it = std::find(members_.begin(), members_.end(), guid);
        members_.erase(it);//HasMember(guid) already check
        if (!members_.empty() && leader_leave)
        {
            OnAppointLeader(*members_.begin());
        }        
        playerid_team_map().erase(guid);
        emp_->emit<EventAfterLeaveTeam>(teamid_, guid);     
        return RET_OK;
    }

    uint32_t Team::KickMember(Guid current_leader, Guid  kick_guid)
    {
        if (leader_id_ != current_leader)
        {
            return RET_TEAM_KICK_NOT_LEADER;
        }
        if (leader_id_ == kick_guid)
        {
            return RET_TEAM_KICK_SELF;
        }
        if (current_leader == kick_guid)
        {
            return RET_TEAM_KICK_SELF;
        }
        RET_CHECK_RET(LeaveTeam(kick_guid));
        return RET_OK;
    }

    uint32_t Team::AppointLeader(Guid current_leader, Guid new_leader)
    {
        if (leader_id_ == new_leader)
        {
            return RET_TEAM_APPOINT_SELF;
        }
        if (!IsMember(new_leader))
        {
            return RET_TEAM_HAS_NOT_TEAM_ID;
        }
        if (leader_id_ != current_leader)
        {
            return RET_TEAM_APPOINT_SELF;
        }
        OnAppointLeader(new_leader);
        return RET_OK;
    }

    void Team::OnAppointLeader(Guid guid)
    {
        auto old_guid = leader_id_;
        leader_id_ = guid;
        emp_->emit<TeamESAppointLeader>(teamid_, old_guid, leader_id_);
    }

    uint32_t Team::DissMiss(Guid current_leader_id)
    {
        if (leader_id() != current_leader_id)
        {
            return RET_TEAM_DISMISS_NOT_LEADER;
        }
        auto& ms = playerid_team_map();
        for (auto& it : members_)
        {
            emp_->emit<EventDissmisTeam>(teamid_, it);
            ms.erase(it);               
        }
        return RET_OK;
    }

    void Team::ClearApplyList()
    {
        applicants_.clear();      
    }

	uint32_t Team::ApplyToTeam(Guid guid)
	{
		if (HasTeam(guid))
		{
			return RET_TEAM_MEMBER_IN_TEAM;
		}
		if (IsFull())
		{
			return RET_TEAM_MEMBERS_FULL;
		}
		RET_CHECK_RET(CheckLimt(guid));
		if (applicants_.size() >= kMaxApplicantSize)
		{
			applicants_.erase(applicants_.begin());
		}
		applicants_.emplace_back(guid);
		return RET_OK;
	}


    uint32_t Team::DelApplicant(Guid applicant_id)
    {
        auto it = std::find(applicants_.begin(), applicants_.end(), applicant_id);
        if (it != applicants_.end())
        {
            applicants_.erase(it);
        }
        return RET_OK;
    }

}//namespace common
