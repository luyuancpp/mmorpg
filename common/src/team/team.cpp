#include "team.h"

#include "src/return_code/return_notice_code.h"

namespace common
{
    Team::Team(const CreateTeamParam& param)
        :
        team_id_(param.team_id_),
        leader_id_(param.leader_id_)
    {
        for (auto& it : param.members)
        {
            OnJoinTeam(it.second);
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

    GameGuid Team::begein_applicant() const
    {
        if (applicant_ids_.empty())
        {
            return kEmptyPlayerId;
        }
        return *applicant_ids_.begin();
    }

    ReturnValue Team::JoinTeam(const TeamMember& m)
    {
        RET_CHECK_RET(TryToJoinTeam(m));
        OnJoinTeam(m);
        return RET_OK;
    }

    ReturnValue Team::TryToJoinTeam(const TeamMember& m)
    {
        if (m.player_id() == kEmptyPlayerId)
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
        if (!members_.empty() && IsLeader(player_id))
        {
            OnAppointLeader(members_.begin()->first);
        }
        members_.erase(player_id);
        return RET_OK;
    }

    void Team::OnJoinTeam(const TeamMember& m)
    {
        members_.emplace(m.player_id(), m);
        RemoveApplicant(m.player_id());
    }

    ReturnValue Team::KickMember(GameGuid current_leader, GameGuid  kick_player_id)
    {
        if (leader_id_ == kick_player_id)
        {
            return RET_TEAM_KICK_SELF;
        }
        if (leader_id_ != current_leader)
        {
            return RET_TEAM_KICK_NOT_LEADER;
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
            return RET_TEAM_MEMBER_NOT_IN_TEAM;
        }

        if (leader_id_ != current_leader)
        {
            return RET_TEAM_APPOINT_LEADER_NOT_LEADER;
        }

        OnAppointLeader(new_leader_player_id);
        return RET_OK;
    }

    void Team::OnAppointLeader(GameGuid new_leader_player_id)
    {
        leader_id_ = new_leader_player_id;
    }

    ReturnValue Team::Apply(const TeamMember& m)
    {
        if (IsFull())
        {
            return RET_TEAM_TEAM_FULL;
        }

        if (members_.find(m.player_id()) != members_.end())
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }

        RET_CHECK_RET(CheckLimt(m));

        auto it = applicants_.find(m.player_id());
        if (it != applicants_.end())
        {
            return RET_TEAM_IN_APPLICANT_LIEST;
        }
        if (applicant_ids_.size() >= kMaxApplicantSize)
        {
            assert(!applicant_ids_.empty());
            applicants_.erase(*applicant_ids_.begin());
            applicant_ids_.erase(applicant_ids_.begin());
        }
        applicants_.emplace(m.player_id(), m);
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
        RET_CHECK_RET(RemoveApplicant(applicant_id));
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
        auto idit = std::find(applicant_ids_.begin(), applicant_ids_.end(), applicant_id);
        if (idit != applicant_ids_.end())
        {
            applicant_ids_.erase(idit);
        }
        return RET_OK;
    }

    void Team::ClearApplyList()
    {
        applicants_.clear();
        applicant_ids_.clear();
    }

    int32_t Team::CheckLimt(const TeamMember& m)
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
}//namespace common
