#ifndef COMMON_SRC_GAME_LOGIC_TEAM_TEAM_H_
#define COMMON_SRC_GAME_LOGIC_TEAM_TEAM_H_

#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "team.pb.h"

#include "src/common_type/common_type.h"
#include "src/event/event.h"
#include "entt/src/entt/entity/entity.hpp"
#include "entt/src/entt/entity/registry.hpp"
#include "src/game_ecs/entity_cast.h"
#include "src/return_code/notice_struct.h"

namespace common
{
    static const std::size_t kMaxApplicantSize{ 20 };
    static const std::size_t kMaxMemberSize{ 5 };
    typedef std::unordered_map<GameGuid, TeamMember> Members;

    //function order get, set is, test action
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
        
        Team(entt::entity team_id, 
            EventManagerPtr& emp, 
            const CreateTeamParam& param, 
            entt::registry* teams_registry);

        GameGuid team_id()const { return entt::to_integral(team_id_); }
        entt::entity to_entityid()const { return team_id_; }
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

        bool HasApplicant(GameGuid applicant_id) const { return applicants_.find(applicant_id) != applicants_.end(); }
        bool HasApply()const { return !applicants_.empty(); }
        bool IsFull()const { return members_.size() >= max_member_size(); }
        bool IsLeader(GameGuid playerid)const { return leader_id_ != kEmptyGameGuid && leader_id_ == playerid; }
        bool InTeam(GameGuid player_guid)const { return members_.find(player_guid) != members_.end(); }

        ReturnValue CheckLimt(const TeamMember& m);
        bool TestApplicantValueEqual()const;

        void OnCreate();
        ReturnValue JoinTeam(const TeamMember& m);
        ReturnValue TryToJoinTeam(const TeamMember& m);
        ReturnValue LeaveTeam(GameGuid playerid);
        ReturnValue KickMember(GameGuid current_leader, GameGuid  nKickplayerid);
        ReturnValue AppointLeader(GameGuid current_leader, GameGuid  new_leader_player_id);
        ReturnValue ApplyForTeam(const TeamMember& m);
        ReturnValue AgreeApplicant(GameGuid applicant_id);
        ReturnValue RemoveApplicant(GameGuid applicant_id);
        void ClearApplyList();

    private:
        void AddMember(const TeamMember& m)
        {
            members_.emplace(m.player_id(), m);
            sequence_players_id_.push_back(m.player_id());
        }

        void OnAppointLeader(GameGuid  new_leader_player_id);
        void RemoveApplicantId(GameGuid  player_id);

        entt::entity team_id_{};
        GameGuid leader_id_{};
        Members members_;
        ApplyMembers applicants_;
        PlayerIdsV applicant_ids_;
        PlayerIdsV sequence_players_id_;
        EventManagerPtr emp_;
        entt::registry* teams_registry_{ nullptr };
    };
}//namespace common

#endif // COMMON_SRC_GAME_LOGIC_TEAM_TEAM_H_