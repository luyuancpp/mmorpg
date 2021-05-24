#ifndef COMMON_SRC_TEAM_TEAM_H_
#define COMMON_SRC_TEAM_TEAM_H_

#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "team.pb.h"

#include "src/common_type/common_type.h"
#include "src/event/event.h"
#include "src/return_code/notice_struct.h"

namespace common
{
    static const std::size_t kMaxApplicantSize{ 20 };
    static const std::size_t kMaxMemberSize{ 5 };
    typedef std::unordered_map<GameGuid, TeamMember> Members;

    struct CreateTeamParam
    {
        GameGuid leader_id_{ 0 };
        const Members members;
        std::string  name_;
        EventManagerPtr emp_;
    };

    class Team
    {
    public:
        using ApplyMembers = std::unordered_map<GameGuid, TeamMember>;
        
        Team(GameGuid team_id, EventManagerPtr& emp, const CreateTeamParam& param);

        GameGuid team_id()const { return team_id_; }
        GameGuid leader_id()const { return leader_id_; }
        const TeamMember& team_member(GameGuid playerid)const;
        std::size_t max_member_size()const { return kMaxMemberSize; }
        std::size_t member_size()const { return members_.size(); }
        bool empty()const { return members_.empty(); }
        std::size_t applicant_size()const
        {
            assert(applicant_ids_.size() == applicants_.size());  return applicants_.size();
        }
        GameGuid first_applicant_id()const;
        const Members& members()const { return members_; }

        ReturnValue JoinTeam(const TeamMember& m);
        ReturnValue TryToJoinTeam(const TeamMember& m);
        ReturnValue LeaveTeam(GameGuid playerid);
        ReturnValue KickMember(GameGuid current_leader, GameGuid  nKickplayerid);
        ReturnValue AppointLeader(GameGuid current_leader, GameGuid  new_leader_player_id);
        ReturnValue ApplyForTeam(const TeamMember& m);
        ReturnValue AgreeApplicant(GameGuid applicant_id);
        ReturnValue RemoveApplicant(GameGuid applicant_id);
        ReturnValue DisMiss();
        void ClearApplyList();

        bool HasApplicant(GameGuid applicant_id) const { return applicants_.find(applicant_id) != applicants_.end(); }
        bool HasApply()const { return !applicants_.empty(); }
        bool IsFull()const { return members_.size() >= max_member_size(); }
        bool IsLeader(GameGuid playerid)const { return leader_id_ != kEmptyGameGuid && leader_id_ == playerid; }
        bool InTeam(GameGuid player_guid)const { return members_.find(player_guid) != members_.end(); }

        ReturnValue CheckLimt(const TeamMember& m);
        bool TestApplicantValueEqual()const;

    protected:
        template<typename E>
        void OnJoinTeam(const TeamMember& m)
        {
            RemoveApplicant(m.player_id());
            members_.emplace(m.player_id(), m);
            sequence_players_id_.push_back(m.player_id());
            assert(members_.size() == sequence_players_id_.size());
            emp_->emit<E>(team_id_, m.player_id());
        }
        void OnAppointLeader(GameGuid  new_leader_player_id);
        void RemoveApplicantId(GameGuid  player_id);

        GameGuid team_id_{ kEmptyGameGuid };
        GameGuid leader_id_{ kEmptyGameGuid };
        Members members_;
        ApplyMembers applicants_;
        PlayerIdsV applicant_ids_;
        PlayerIdsV sequence_players_id_;
        EventManagerPtr emp_;
    };
}//namespace common

#endif // COMMON_SRC_TEAM_TEAM_H_