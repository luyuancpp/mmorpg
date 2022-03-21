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
#include "src/game_logic/entity/entity.h"

namespace common
{
    static const std::size_t kMaxApplicantSize{ 20 };
    static const std::size_t kMaxMemberSize{ 5 };

    //function order get, set is, test action
    struct CreateTeamP
    {
        Guid leader_id_{ 0 };
        const UInt64Set members;
    };

    struct TeamsP
    {
        entt::entity teamid_{};
        entt::entity teams_entity_id_{};//manager id
        EventManagerPtr& emp_;
        entt::registry* teams_registry_{ nullptr };
    };

    using PlayerTeamMap = std::unordered_map<Guid, entt::entity>;

class Team
{
public:
    using ApplyMembers = std::unordered_set<Guid>;
        
    Team(const CreateTeamP& param, 
        const TeamsP& teams_param);

    inline Guid team_id()const { return entt::to_integral(teamid_); }
    inline entt::entity to_entityid()const { return teamid_; }
    inline Guid leader_id()const { return leader_id_; }
    inline std::size_t max_member_size()const { return kMaxMemberSize; }
    inline std::size_t member_size()const { return members_.size(); }
    inline bool empty()const { return members_.empty(); }
    inline std::size_t applicant_size()const{  return applicants_.size();}
    Guid first_applicant()const;
    inline const GuidVector& members()const { return members_; }
    inline PlayerTeamMap& playerid_team_map() const { return teams_registry_->get<PlayerTeamMap>(teams_entity_id_); }

    inline bool IsApplicant(Guid guid) const { return std::find(applicants_.begin(), applicants_.end(), guid) != applicants_.end();}
    inline bool IsApplicantEmpty()const { return !applicants_.empty(); }
    inline bool IsFull()const { return members_.size() >= max_member_size(); }
    inline bool IsLeader(Guid guid)const { return leader_id_ == guid; }
    inline bool IsMember(Guid guid)const { return std::find(members_.begin(), members_.end(), guid) != members_.end(); }
   
    uint32_t CheckLimt(Guid  guid);

    uint32_t JoinTeam(Guid  guid);
    uint32_t LeaveTeam(Guid guid);
    uint32_t KickMember(Guid current_leader, Guid  nKickplayerid);
    uint32_t AppointLeader(Guid current_leader, Guid  new_leader_guid);
    uint32_t DissMiss(Guid current_leader_id);
    void ClearApplyList();
    uint32_t ApplyToTeam(Guid guid);
    uint32_t DelApplicant(Guid applicant_id);
    
private:
    inline bool HasTeam(Guid guid) const { return playerid_team_map().find(guid) != playerid_team_map().end(); }

    void OnAppointLeader(Guid  new_leader_guid);
   
    entt::entity teamid_{};
    entt::entity teams_entity_id_{};//manager id
    Guid leader_id_{};
    GuidVector members_;
    GuidVector applicants_;
    EventManagerPtr emp_;
    entt::registry* teams_registry_{ nullptr };
};
}//namespace common

#endif // COMMON_SRC_GAME_LOGIC_TEAM_TEAM_H_