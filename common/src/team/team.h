#ifndef COMMON_SRC_TEAM_Team
#define COMMON_SRC_TEAM_Team

#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "team.pb.h"

#include "src/common_type/common_type.h"
#include "src/return_code/notice_struct.h"

namespace common
{
    static const GameGuid kEmptyPlayerId{ 0 };
    static const GameGuid kEmptyTeamId = 0;
    static const std::size_t kMaxApplicantSize = 20;
    typedef std::unordered_map<GameGuid, TeamMember> Members;

    struct CreateTeamParam
    {
        GameGuid team_id_{ 0 };
        GameGuid leader_id_{ 0 };
        const Members& members;
        std::string  name_;
    };

    class Team
    {
    public:
        using applymap_type = std::unordered_map<GameGuid, TeamMember>;
        
        Team(const CreateTeamParam& param);

        GameGuid team_id()const { return team_id_; }
        GameGuid leader_id()const { return leader_id_; }
        const TeamMember& team_member(GameGuid playerid)const;
        std::size_t max_member_size()const { return 5; }
        std::size_t member_size()const { return members_.size(); }
        bool empty()const { return members_.empty(); }
        std::size_t applicant_size()const
        {
            assert(applicant_ids_.size() == applicants_.size());  return applicants_.size();
        }
        GameGuid begein_applicant()const;
        const Members& members()const { return members_; }

        ReturnValue JoinTeam(const TeamMember& m);
        ReturnValue TryToJoinTeam(const TeamMember& m);
        ReturnValue LeaveTeam(GameGuid playerid);
        ReturnValue KickMember(GameGuid current_leader, GameGuid  nKickplayerid);
        ReturnValue AppointLeader(GameGuid current_leader, GameGuid  new_leader_player_id);
        ReturnValue Apply(const TeamMember& m);
        ReturnValue AgreeApplicant(GameGuid applicant_id);
        ReturnValue RemoveApplicant(GameGuid applicant_id);
        void ClearApplyList();

        bool HasApplicant(GameGuid nApplyplayerid) const { return applicants_.find(nApplyplayerid) != applicants_.end(); }
        bool HasApply()const { return !applicants_.empty(); }
        bool IsFull()const { return members_.size() >= max_member_size(); }
        bool IsLeader(GameGuid playerid)const { return leader_id_ != kEmptyPlayerId && leader_id_ == playerid; }
        bool InTeam(GameGuid player_guid)const { return members_.find(player_guid) != members_.end(); }

        int32_t CheckLimt(const TeamMember& m);
        bool TestApplicantValueEqual()const;

    protected:
        void OnJoinTeam(const TeamMember& m);
        void OnAppointLeader(GameGuid  new_leader_player_id);

        GameGuid team_id_{ kEmptyTeamId };
        GameGuid leader_id_{ kEmptyPlayerId };
        Members members_;
        applymap_type applicants_;
        PlayerIdsV applicant_ids_;
    };
}//namespace common

#endif // COMMON_SRC_Team