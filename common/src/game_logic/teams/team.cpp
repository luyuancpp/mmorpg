#include "team.h"

#include "src/return_code/error_code.h"
#include "src/game_logic/teams/team_event.h"
#include "src/game_logic/game_registry.h"

namespace common
{
    Team::Team(const CreateTeamParam& param,
        const TeamsParam& teams_param)
        : team_id_(teams_param.team_id_),
          teams_entity_id_(teams_param.teams_entity_id_),
          leader_id_(param.leader_id_),
          emp_(teams_param.emp_),
          teams_registry_(teams_param.teams_registry_)
    {
        for (auto& it : param.members)
        {
            AddMember(it);
        }
    }

    Guid Team::first_applicant_id() const
    {
        if (applicant_ids_.empty())
        {
            return kEmptyGuid;
        }
        return *applicant_ids_.begin();
    }

    uint32_t Team::CheckLimt(Guid  guid)
    {
        return RET_OK;
    }

    bool Team::TestApplicantValueEqual() const
    {
        for (auto it : applicant_ids_)
        {
            if (applicants_.find(it) == applicants_.end())
            {
                return false;
            }
        }
        return true;
    }

    void Team::OnCreate()
    {
        auto& ms = playerid_team_map();
        for (auto& it : members_)
        {
            ms.emplace(it, team_id_);
            emp_->emit<TeamESJoinTeam>(team_id_, it);
        }
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
        //assert(members_.find(guid) == members_.end());
        RemoveApplicant(guid);
        AddMember(guid);
        assert(members_.size() == sequence_players_id_.size());
        auto& ms = playerid_team_map();
        ms.emplace(guid, team_id_);
        emp_->emit< TeamESJoinTeam>(team_id_, guid);
        return RET_OK;
    }

    uint32_t Team::LeaveTeam(Guid guid)
    {
        auto it = members_.find(guid);
        if (it == members_.end())
        {
            return RET_TEAM_MEMBER_NOT_IN_TEAM;
        }
        bool leader_leave = IsLeader(guid);
        if (leader_leave)
        {
            emp_->emit<TeamESLeaderLeaveTeam>(team_id_, guid);
        }
        members_.erase(guid);
        auto sit = std::find(sequence_players_id_.begin(), sequence_players_id_.end(), guid);
        if (sit != sequence_players_id_.end())
        {
            sequence_players_id_.erase(sit);
        }
        assert(members_.size() == sequence_players_id_.size());
        if (!sequence_players_id_.empty() && leader_leave)
        {
            OnAppointLeader(*sequence_players_id_.begin());
        }
        emp_->emit<TeamESLeaveTeam>(team_id_, guid);
        playerid_team_map().erase(guid);
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
        auto it = members_.find(kick_guid);
        if (it == members_.end())
        {
            return RET_TEAM_MEMBER_NOT_IN_TEAM;
        }
        RET_CHECK_RET(LeaveTeam(kick_guid));
        return RET_OK;
    }

    uint32_t Team::AppointLeader(Guid current_leader, Guid new_leader_guid)
    {
        if (leader_id_ == new_leader_guid)
        {
            return RET_TEAM_APPOINT_SELF;
        }
        if (!InMyTeam(new_leader_guid))
        {
            return RET_TEAM_HAS_NOT_TEAM_ID;
        }
        if (leader_id_ != current_leader)
        {
            return RET_TEAM_APPOINT_SELF;
        }
        OnAppointLeader(new_leader_guid);
        return RET_OK;
    }

    void Team::OnAppointLeader(Guid new_leader_guid)
    {
        auto old_leader_guid = leader_id_;
        leader_id_ = new_leader_guid;
        emp_->emit<TeamESAppointLeader>(team_id_, old_leader_guid, leader_id_);
    }

    void Team::RemoveApplicantId(Guid guid)
    {
        auto idit = std::find(applicant_ids_.begin(), applicant_ids_.end(), guid);
        if (idit == applicant_ids_.end())
        {
            return;
        }
        applicant_ids_.erase(idit);
    }

    uint32_t Team::ApplyForTeam(Guid guid)
    {
        if (HasTeam(guid))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        if (IsFull())
        {
            return RET_TEAM_MEMBERS_FULL;
        }
        //assert(members_.find(guid) == members_.end());

        RET_CHECK_RET(CheckLimt(guid));
        RemoveApplicantId(guid);
        if (applicant_ids_.size() >= kMaxApplicantSize)
        {
            assert(!applicant_ids_.empty());
            applicants_.erase(*applicant_ids_.begin());
            applicant_ids_.erase(applicant_ids_.begin());
        }
        applicants_.emplace(guid);
        applicant_ids_.emplace_back(guid);
        return RET_OK;
    }

    uint32_t Team::AgreeApplicant(Guid applicant_id)
    {
        auto ret = JoinTeam(applicant_id);
        if (ret != RET_OK && ret != RET_TEAM_MEMBERS_FULL)
        {
            RemoveApplicant(applicant_id);
        }
        return ret;
    }

    uint32_t Team::RemoveApplicant(Guid applicant_id)
    {
        applicants_.erase(applicant_id);
        RemoveApplicantId(applicant_id);
        return RET_OK;
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
            emp_->emit<TeamESLeaveTeam>(team_id_, it);
            ms.erase(it);   
        }
        emp_->emit<TeamESLeaderDismissTeam>(team_id_, kEmptyGuid);
        return RET_OK;
    }

    void Team::ClearApplyList()
    {
        applicants_.clear();
        applicant_ids_.clear();
        emp_->emit<TeamESClearApplyList>(team_id_);        
    }
}//namespace common
