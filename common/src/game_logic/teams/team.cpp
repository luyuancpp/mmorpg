#include "team.h"

#include "src/return_code/return_notice_code.h"
#include "src/game_logic/teams/team_event.h"

namespace common
{
    Team::Team(GameGuid team_id, EventManagerPtr& emp, const CreateTeamParam& param)
        : team_id_(team_id),
          leader_id_(param.leader_id_),
          emp_(emp)
    {
        for (auto& it : param.members)
        {
            OnJoinTeam<TeamEventStructCreateTeamJoinTeam>(it.second);
        }
    }

    const TeamMember& Team::team_member(GameGuid player_id)const
    {
        auto mit = members_.find(player_id);
        if (mit == members_.end())
        {
            static TeamMember m;
            return m;
        }
        return mit->second;
    }

    GameGuid Team::first_applicant_id() const
    {
        if (applicant_ids_.empty())
        {
            return kEmptyGameGuid;
        }
        return *applicant_ids_.begin();
    }

    ReturnValue Team::CheckLimt(const TeamMember& m)
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

    ReturnValue Team::JoinTeam(const TeamMember& m)
    {
        RET_CHECK_RET(TryToJoinTeam(m));
        OnJoinTeam<TeamEventStructJoinTeam>(m);
        return RET_OK;
    }

    ReturnValue Team::TryToJoinTeam(const TeamMember& m)
    {
        if (m.player_id() == kEmptyGameGuid)
        {
            return RET_TEAM_PLAEYR_ID;
        }
        if (IsFull())
        {
            return RET_TEAM_MEMBERS_FULL;
        }
        auto it = members_.find(m.player_id());
        if (it != members_.end())
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        return RET_OK;
    }

    ReturnValue Team::LeaveTeam(GameGuid player_id)
    {
        auto it = members_.find(player_id);
        if (it == members_.end())
        {
            return RET_TEAM_MEMBER_NOT_IN_TEAM;
        }
        bool leader_leave = IsLeader(player_id);
        members_.erase(player_id);
        auto sit = std::find(sequence_players_id_.begin(), sequence_players_id_.end(), player_id);
        if (sit != sequence_players_id_.end())
        {
            sequence_players_id_.erase(sit);
        }
        assert(members_.size() == sequence_players_id_.size());
        if (!sequence_players_id_.empty() && leader_leave)
        {
            OnAppointLeader(*sequence_players_id_.begin());
        }
        emp_->emit<TeamEventStructLeaveTeam>(team_id_, player_id);
        if (members_.empty())
        {
            DisMiss();
        }
        return RET_OK;
    }



    ReturnValue Team::KickMember(GameGuid current_leader, GameGuid  kick_player_id)
    {
        if (leader_id_ != current_leader)
        {
            return RET_TEAM_KICK_NOT_LEADER;
        }
        if (leader_id_ == kick_player_id)
        {
            return RET_TEAM_KICK_SELF;
        }
        if (current_leader == kick_player_id)
        {
            return RET_TEAM_KICK_SELF;
        }
        auto it = members_.find(kick_player_id);
        if (it == members_.end())
        {
            return RET_TEAM_MEMBER_NOT_IN_TEAM;
        }
        RET_CHECK_RET(LeaveTeam(kick_player_id));
        return RET_OK;
    }

    ReturnValue Team::AppointLeader(GameGuid current_leader, GameGuid new_leader_player_id)
    {
        if (leader_id_ == new_leader_player_id)
        {
            return RET_TEAM_APPOINT_SELF;
        }
        if (!InTeam(new_leader_player_id))
        {
            return RET_TEAM_HAS_NOT_TEAM_ID;
        }
        if (leader_id_ != current_leader)
        {
            return RET_TEAM_APPOINT_SELF;
        }
        OnAppointLeader(new_leader_player_id);
        return RET_OK;
    }

    void Team::OnAppointLeader(GameGuid new_leader_player_id)
    {
        auto old_leader_player_id = leader_id_;
        leader_id_ = new_leader_player_id;
        emp_->emit<TeamEventStructAppointLeader>(team_id_, old_leader_player_id, leader_id_);
    }

    void Team::RemoveApplicantId(GameGuid player_id)
    {
        auto idit = std::find(applicant_ids_.begin(), applicant_ids_.end(), player_id);
        if (idit == applicant_ids_.end())
        {
            return;
        }
        applicant_ids_.erase(idit);
    }

    ReturnValue Team::ApplyForTeam(const TeamMember& m)
    {
        if (IsFull())
        {
            return RET_TEAM_MEMBERS_FULL;
        }

        if (members_.find(m.player_id()) != members_.end())
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }

        RET_CHECK_RET(CheckLimt(m));

        auto it = applicants_.find(m.player_id());
        if (it != applicants_.end())
        {
            RemoveApplicantId(m.player_id());
        }
        if (applicant_ids_.size() >= kMaxApplicantSize)
        {
            assert(!applicant_ids_.empty());
            applicants_.erase(*applicant_ids_.begin());
            applicant_ids_.erase(applicant_ids_.begin());
        }
        applicants_.emplace(m.player_id(), m);
        applicant_ids_.emplace_back(m.player_id());
        return RET_OK;
    }

    ReturnValue Team::AgreeApplicant(GameGuid applicant_id)
    {
        auto it = applicants_.find(applicant_id);
        if (it == applicants_.end())
        {
            return RET_TEAM_NOT_IN_APPLICANTS;
        }
        TeamMember m;
        m.CopyFrom(it->second);
        RET_CHECK_RET(JoinTeam(m));
        return RET_OK;
    }

    ReturnValue Team::RemoveApplicant(GameGuid applicant_id)
    {
        auto it = applicants_.find(applicant_id);
        if (it == applicants_.end())
        {
            return RET_TEAM_NOT_IN_APPLICANT_LIEST;
        }
        applicants_.erase(applicant_id);
        RemoveApplicantId(applicant_id);
        return RET_OK;
    }

    ReturnValue Team::DisMiss()
    {
        emp_->emit<TeamEventStructDismissTeamOnTeamMemberEmpty>(team_id_);
        return RET_OK;
    }

    void Team::ClearApplyList()
    {
        applicants_.clear();
        applicant_ids_.clear();
    }
}//namespace common
