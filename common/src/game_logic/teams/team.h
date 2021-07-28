#ifndef COMMON_SRC_GAME_LOGIC_TEAM_TEAM_H_
#define COMMON_SRC_GAME_LOGIC_TEAM_TEAM_H_

#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/common_type/common_type.h"
#include "src/event/event.h"
#include "entt/src/entt/entity/entity.hpp"
#include "entt/src/entt/entity/registry.hpp"
#include "src/game_logic/entity_cast.h"

namespace common
{
    static const std::size_t kMaxApplicantSize{ 20 };
    static const std::size_t kMaxMemberSize{ 5 };

    //function order get, set is, test action
    struct CreateTeamParam
    {
        GameGuid leader_id_{ 0 };
        const UI64USet members;
    };

    struct TeamsParam
    {
        entt::entity team_id_{};
        entt::entity teams_entity_id_{};//manager id
        EventManagerPtr& emp_;
        entt::registry* teams_registry_{ nullptr };
    };

    struct PlayerInTeamF
    {
        using FunctionType = std::function<bool(GameGuid)>;
        FunctionType cb_;
    };

    using PlayerIdTeamIdMap = std::unordered_map<GameGuid, entt::entity>;

class Team
{
public:
    using ApplyMembers = std::unordered_set<GameGuid>;
        
    Team(const CreateTeamParam& param, 
        const TeamsParam& teams_param);

    GameGuid team_id()const { return entt::to_integral(team_id_); }
    entt::entity to_entityid()const { return team_id_; }
    inline GameGuid leader_id()const { return leader_id_; }
    std::size_t max_member_size()const { return kMaxMemberSize; }
    std::size_t member_size()const { return members_.size(); }
    bool empty()const { return members_.empty(); }
    std::size_t applicant_size()const
    {
        assert(applicant_ids_.size() == applicants_.size());  return applicants_.size();
    }
    GameGuid first_applicant_id()const;
    const UI64USet& members()const { return members_; }
    inline PlayerIdTeamIdMap& playerid_team_map() { return teams_registry_->get<PlayerIdTeamIdMap>(teams_entity_id_); }

    bool HasApplicant(GameGuid applicant_id) const { return applicants_.find(applicant_id) != applicants_.end(); }
    inline bool HasApply()const { return !applicants_.empty(); }
    inline bool IsFull()const { return members_.size() >= max_member_size(); }
    inline bool IsLeader(GameGuid player_id)const { assert(leader_id_ != kEmptyGameGuid); return leader_id_ == player_id; }
    inline bool InMyTeam(GameGuid player_id)const { return members_.find(player_id) != members_.end(); }
    inline bool HasTeam(GameGuid player_id) const { return teams_registry_->get<PlayerInTeamF>(team_id_).cb_(player_id); }

    uint32_t CheckLimt(GameGuid  player_id);
    bool TestApplicantValueEqual()const;

    void OnCreate();
    uint32_t JoinTeam(GameGuid  player_id);
    uint32_t LeaveTeam(GameGuid player_id);
    uint32_t KickMember(GameGuid current_leader, GameGuid  nKickplayerid);
    uint32_t AppointLeader(GameGuid current_leader, GameGuid  new_leader_player_id);
    uint32_t ApplyForTeam(GameGuid player_id);
    uint32_t AgreeApplicant(GameGuid applicant_id);
    uint32_t RemoveApplicant(GameGuid applicant_id);
    uint32_t DissMiss(GameGuid current_leader_id);
    void ClearApplyList();

private:
    void AddMember(GameGuid  player_id)
    {
        members_.emplace(player_id);
        sequence_players_id_.push_back(player_id);
    }

    void OnAppointLeader(GameGuid  new_leader_player_id);
    void RemoveApplicantId(GameGuid  player_id);

    entt::entity team_id_{};
    entt::entity teams_entity_id_{};//manager id
    GameGuid leader_id_{};
    UI64USet members_;
    ApplyMembers applicants_;
    PlayerIdsV applicant_ids_;
    PlayerIdsV sequence_players_id_;
    EventManagerPtr emp_;
    entt::registry* teams_registry_{ nullptr };
};
}//namespace common

#endif // COMMON_SRC_GAME_LOGIC_TEAM_TEAM_H_