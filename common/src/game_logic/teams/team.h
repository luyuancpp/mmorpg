#pragma once

#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/common_type/common_type.h"
#include "entt/src/entt/entity/entity.hpp"
#include "entt/src/entt/entity/registry.hpp"


static const std::size_t kMaxApplicantSize{ 20 };
static const std::size_t kMaxMemberSize{ 5 };

//function order get, set is, test action
struct CreateTeamP
{
    Guid leader_id_{ 0 };
    const UInt64Set members;
};

class Team
{
public:
    using ApplyMembers = std::unordered_set<Guid>;
        
    Team(const CreateTeamP& param, entt::entity teamid);

    inline Guid team_id()const { return entt::to_integral(teamid_); }
    inline entt::entity to_entityid()const { return teamid_; }
    inline Guid leader_id()const { return leader_id_; }
    inline std::size_t max_member_size()const { return kMaxMemberSize; }
    inline std::size_t member_size()const { return members_.size(); }
    inline bool empty()const { return members_.empty(); }
    inline std::size_t applicant_size()const{  return applicants_.size();}
    Guid first_applicant()const;
    inline const GuidVector& members()const { return members_; }

    inline bool IsApplicant(Guid guid) const { return std::find(applicants_.begin(), applicants_.end(), guid) != applicants_.end();}
    inline bool IsApplicantEmpty()const { return !applicants_.empty(); }
    inline bool IsFull()const { return members_.size() >= max_member_size(); }
    inline bool IsLeader(Guid guid)const { return leader_id_ == guid; }
    inline bool IsMember(Guid guid)const { return std::find(members_.begin(), members_.end(), guid) != members_.end(); }
   
    uint32_t JoinTeam(Guid  guid);
    uint32_t LeaveTeam(Guid guid);
    uint32_t KickMember(Guid current_leader, Guid  nKickplayerid);
    uint32_t AppointLeader(Guid current_leader, Guid  new_leader_guid);
    uint32_t Disbanded(Guid current_leader_id);
    void ClearApplyList();
    uint32_t ApplyToTeam(Guid guid);
    uint32_t DelApplicant(Guid applicant_id);
    
public:
    inline bool HasTeam(Guid guid) const;
    void AddMemeber(Guid guid);
    void DelMember(Guid guid);
    void OnAppointLeader(Guid  new_leader_guid);
   
    Guid leader_id_{};
    entt::entity teamid_{};
    GuidVector members_;
    GuidVector applicants_;
};
