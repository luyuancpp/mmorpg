#pragma once

#include "type_define/type_define.h"
#include "thread_local/storage.h"
#include "util/snow_flake.h"

#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "type_define/type_define.h"
#include "entt/src/entt/entity/entity.hpp"
#include "entt/src/entt/entity/registry.hpp"


static constexpr std::size_t kMaxApplicantSize{ 20 };

static constexpr std::size_t kFiveMemberMaxSize{ 5 };
static constexpr std::size_t kTenMemberMaxSize{ 10 };


//function order get, set is, test action
struct CreateTeamP
{
	Guid leader_id_{ 0 };
	const UInt64Set member_list;
	std::size_t team_type_size_{ kFiveMemberMaxSize };
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

	void OnAppointLeader(const Guid new_leader_guid) { leader_id_ = new_leader_guid; }


	Guid leader_id_{ kInvalidGuid };
	entt::entity team_id_{ entt::null };
	GuidVector members_;
	GuidVector applicants_;
	std::size_t team_type_size_{ kFiveMemberMaxSize };
};


static constexpr std::size_t kMaxTeamSize = 10000;

class TeamSystem final
{
public:
    ~TeamSystem();

    static std::size_t team_size();
    static std::size_t member_size(Guid team_id);
    static std::size_t applicant_size_by_player_id(Guid guid);
    static std::size_t applicant_size_by_team_id(Guid team_id);
    static std::size_t players_size();
    static Guid GetTeamId(Guid guid);
    [[nodiscard]] Guid last_team_id() const;
    static Guid get_leader_id_by_team_id(Guid team_id);
    static Guid get_leader_id_by_player_id(Guid guid);
    static Guid first_applicant(Guid team_id);

    [[nodiscard]] static bool IsTeamListMax();
    static bool IsTeamFull(Guid team_id);
    static bool HasMember(Guid team_id, Guid guid);
    static bool HasTeam(Guid guid);
    static bool IsApplicant(Guid team_id, Guid guid);

    uint32_t CreateTeam(const CreateTeamP& param);
    static uint32_t JoinTeam(Guid team_id, Guid guid);
    static uint32_t JoinTeam(const UInt64Set& member_list, Guid team_id);
    static uint32_t LeaveTeam(Guid guid);
    static uint32_t KickMember(Guid team_id, Guid current_leader_id, Guid be_kick_id);
    static uint32_t Disbanded(Guid team_id, Guid current_leader_id);
    static uint32_t DisbandedTeamNoLeader(Guid team_id);
    static uint32_t AppointLeader(Guid team_id, Guid current_leader_id, Guid new_leader_id);
    static uint32_t ApplyToTeam(Guid team_id, Guid guid);
    static uint32_t DelApplicant(Guid team_id, Guid apply_guid);
    static void ClearApplyList(Guid team_id);

    static uint32_t AddMember(Guid team_id, Guid guid);
    static uint32_t DelMember(Guid team_id, Guid guid);

private:
    [[nodiscard]] static uint32_t CheckMemberInTeam(const UInt64Set& member_list);
    static void EraseTeam(entt::entity team_id);

    Guid last_team_id_{0}; //for test
};

