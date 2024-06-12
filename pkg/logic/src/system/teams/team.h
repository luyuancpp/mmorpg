#pragma once

#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "type_define/type_define.h"
#include "entt/src/entt/entity/entity.hpp"
#include "entt/src/entt/entity/registry.hpp"


static constexpr std::size_t kMaxApplicantSize{20};

static constexpr std::size_t kFiveMemberMaxSize{5};
static constexpr std::size_t kTenMemberMaxSize{10};


//function order get, set is, test action
struct CreateTeamP
{
    Guid leader_id_{0};
    const UInt64Set member_list;
    std::size_t team_type_size_{kFiveMemberMaxSize};
};

class Team
{
public:
    inline entt::entity to_entity_id() const { return team_id_; }
    inline Guid leader_id() const { return leader_id_; }
    inline std::size_t max_member_size() const { return team_type_size_; }
    inline std::size_t member_size() const { return members_.size(); }
    inline bool empty() const { return members_.empty(); }
    inline std::size_t applicant_size() const { return applicants_.size(); }

    inline bool IsApplicant(const Guid guid) const { return std::find(applicants_.begin(), applicants_.end(), guid) != applicants_.end(); }
    inline bool IsFull() const { return members_.size() >= max_member_size(); }
    inline bool IsLeader(const Guid guid) const { return leader_id_ == guid; }
    inline bool HasMember(const Guid guid) const { return std::find(members_.begin(), members_.end(), guid) != members_.end(); }

    void AddMember(Guid guid);
    void DelMember(Guid guid);
    void OnAppointLeader(Guid new_leader_guid);

    Guid leader_id_{kInvalidGuid};
    entt::entity team_id_{entt::null};
    GuidVector members_;
    GuidVector applicants_;
    std::size_t team_type_size_{kFiveMemberMaxSize};
};
