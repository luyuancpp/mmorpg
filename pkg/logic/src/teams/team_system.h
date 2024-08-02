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

#include "thread_local/storage_common_logic.h"
#include "thread_local/storage.h"
#include "constants/tips_id_constants.h"
#include "util/game_registry.h"

#include "proto/logic/component/team_comp.pb.h"

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

TeamSystem::~TeamSystem()
{
	for (const auto& [fst, snd] : tlsCommonLogic.GetPlayerList())
	{
		LeaveTeam(fst);
	}
}

std::size_t TeamSystem::team_size()
{
	return tls.registry.storage<Team>().size();
}

Guid TeamSystem::last_team_id() const
{
	return last_team_id_;
}

bool TeamSystem::IsTeamListMax()
{
	return team_size() >= kMaxTeamSize;
}

std::size_t TeamSystem::member_size(const Guid team_id)
{
	const auto team = entt::to_entity(team_id);
	if (!tls.registry.valid(team))
	{
		return 0;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team);
	if (nullptr == try_team)
	{
		return 0;
	}
	return try_team->member_size();
}

std::size_t TeamSystem::applicant_size_by_player_id(const Guid guid)
{
	const auto team_id = GetTeamId(guid);
	return applicant_size_by_team_id(team_id);
}

std::size_t TeamSystem::applicant_size_by_team_id(const Guid team_id)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return 0;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return 0;
	}
	return try_team->applicant_size();
}

std::size_t TeamSystem::players_size()
{
	return tls.registry.storage<TeamId>().size();
}

Guid TeamSystem::GetTeamId(const Guid guid)
{
	const auto pit = tlsCommonLogic.GetPlayerList().find(guid);
	if (pit == tlsCommonLogic.GetPlayerList().end())
	{
		return entt::null_t();
	}
	const auto* try_team_id = tls.registry.try_get<TeamId>(pit->second);
	if (nullptr == try_team_id)
	{
		return entt::null_t();
	}
	return try_team_id->team_id();
}

Guid TeamSystem::get_leader_id_by_team_id(const Guid team_id)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kInvalidGuid;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kInvalidGuid;
	}
	return try_team->leader_id();
}

Guid TeamSystem::get_leader_id_by_player_id(const Guid guid)
{
	return get_leader_id_by_team_id(GetTeamId(guid));
}

Guid TeamSystem::first_applicant(const Guid team_id)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kInvalidGuid;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kInvalidGuid;
	}
	if (try_team->applicants_.empty())
	{
		return kInvalidGuid;
	}
	return *try_team->applicants_.begin();
}

bool TeamSystem::IsTeamFull(const Guid team_id)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return false;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return false;
	}
	return try_team->IsFull();
}

bool TeamSystem::HasMember(const Guid team_id, const Guid guid)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return false;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return false;
	}
	return try_team->HasMember(guid);
}

bool TeamSystem::HasTeam(const Guid guid)
{
	const auto pit = tlsCommonLogic.GetPlayerList().find(guid);
	if (pit == tlsCommonLogic.GetPlayerList().end())
	{
		return false;
	}
	return tls.registry.any_of<TeamId>(pit->second);
}

bool TeamSystem::IsApplicant(const Guid team_id, const Guid guid)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return false;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return false;
	}
	return try_team->IsApplicant(guid);
}

uint32_t TeamSystem::CreateTeam(const CreateTeamP& param)
{
	if (IsTeamListMax())
	{
		return kRetTeamListMaxSize;
	}
	if (HasTeam(param.leader_id_))
	{
		return kRetTeamMemberInTeam;
	}
	if (param.member_list.size() > param.team_type_size_)
	{
		return kRetTeamCreateTeamMaxMemberSize;
	}
	RET_CHECK_RETURN(CheckMemberInTeam(param.member_list))
		const auto team_entity = tls.registry.create();
	auto& team = tls.registry.emplace<Team>(team_entity);
	team.leader_id_ = param.leader_id_;
	team.team_id_ = team_entity;
	for (const auto& member_it : param.member_list)
	{
		AddMember(entt::to_integral(team_entity), member_it);
	}
	last_team_id_ = entt::to_integral(team_entity);
	return kOK;
}

uint32_t TeamSystem::JoinTeam(const Guid team_id, const Guid guid)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (HasTeam(guid))
	{
		return kRetTeamMemberInTeam;
	}
	if (try_team->IsFull())
	{
		return kRetTeamMembersFull;
	}
	if (const auto applicant_it = std::find(try_team->applicants_.begin(), try_team->applicants_.end(), guid);
		applicant_it != try_team->applicants_.end())
	{
		try_team->applicants_.erase(applicant_it);
	}
	AddMember(team_id, guid);
	return kOK;
}

uint32_t TeamSystem::JoinTeam(const UInt64Set& member_list, const Guid team_id)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (try_team->max_member_size() - try_team->member_size() < member_list.size())
	{
		return kRetTeamJoinTeamMemberListToMax;
	}

	RET_CHECK_RETURN(CheckMemberInTeam(member_list))
		for (const auto& member_it : member_list)
		{
			RET_CHECK_RETURN(JoinTeam(team_id, member_it))
		}
	return kOK;
}

uint32_t TeamSystem::CheckMemberInTeam(const UInt64Set& member_list)
{
	for (const auto& member_it : member_list)
	{
		if (HasTeam(member_it))
		{
			return kRetTeamMemberInTeam;
		}
	}
	return kOK;
}

uint32_t TeamSystem::LeaveTeam(const Guid guid)
{
	const auto team_id = GetTeamId(guid);
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (!try_team->HasMember(guid))
	{
		return kRetTeamMemberNotInTeam;
	}
	const bool is_leader_leave = try_team->IsLeader(guid);
	DelMember(team_id, guid);
	if (!try_team->members_.empty() && is_leader_leave)
	{
		try_team->OnAppointLeader(*try_team->members_.begin());
	}
	if (try_team->empty())
	{
		EraseTeam(try_team->to_entity_id());
	}
	return kOK;
}

uint32_t TeamSystem::KickMember(const Guid team_id, const Guid current_leader_id, const Guid be_kick_id)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (try_team->leader_id_ != current_leader_id)
	{
		return kRetTeamKickNotLeader;
	}
	if (try_team->leader_id_ == be_kick_id)
	{
		return kRetTeamKickSelf;
	}
	if (current_leader_id == be_kick_id)
	{
		return kRetTeamKickSelf;
	}
	if (!try_team->HasMember(be_kick_id))
	{
		return kRetTeamMemberNotInTeam;
	}
	DelMember(team_id, be_kick_id);
	return kOK;
}

uint32_t TeamSystem::Disbanded(const Guid team_id, const Guid current_leader_id)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (try_team->leader_id() != current_leader_id)
	{
		return kRetTeamDismissNotLeader;
	}
	const auto temp_member = try_team->members_;
	for (const auto& member_it : temp_member)
	{
		DelMember(team_id, member_it);
	}
	EraseTeam(team_entity);
	return kOK;
}

uint32_t TeamSystem::DisbandedTeamNoLeader(const Guid team_id)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	return Disbanded(team_id, try_team->leader_id());
}

uint32_t TeamSystem::AppointLeader(const Guid team_id, const Guid current_leader_id, const Guid new_leader_id)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (try_team->leader_id_ == new_leader_id)
	{
		return kRetTeamAppointSelf;
	}
	if (!try_team->HasMember(new_leader_id))
	{
		return kRetTeamHasNotTeamId;
	}
	if (try_team->leader_id_ != current_leader_id)
	{
		return kRetTeamAppointSelf;
	}
	try_team->OnAppointLeader(new_leader_id);
	return kOK;
}

uint32_t TeamSystem::ApplyToTeam(Guid team_id, Guid guid)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (HasTeam(guid))
	{
		return kRetTeamMemberInTeam;
	}
	if (try_team->IsFull())
	{
		return kRetTeamMembersFull;
	}
	if (try_team->applicants_.size() >= kMaxApplicantSize)
	{
		try_team->applicants_.erase(try_team->applicants_.begin());
	}
	try_team->applicants_.emplace_back(guid);
	return kOK;
}

uint32_t TeamSystem::DelApplicant(Guid team_id, Guid guid)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (const auto app_it = std::find(try_team->applicants_.begin(), try_team->applicants_.end(), guid);
		app_it != try_team->applicants_.end())
	{
		try_team->applicants_.erase(app_it);
	}
	return kOK;
}

void TeamSystem::ClearApplyList(const Guid team_id)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return;
	}
	auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return;
	}
	try_team->applicants_.clear();
}

void TeamSystem::EraseTeam(entt::entity team_id)
{
	Destroy(tls.registry, team_id);
}


uint32_t TeamSystem::AddMember(Guid team_id, Guid guid)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}

	const auto pit = tlsCommonLogic.GetPlayerList().find(guid);
	if (pit == tlsCommonLogic.GetPlayerList().end())
	{
		return kRetTeamPlayerNotFound;
	}
	try_team->members_.emplace_back(guid);
	tls.registry.emplace<TeamId>(pit->second).set_team_id(entt::to_integral(team_id));
	return kOK;
}

uint32_t TeamSystem::DelMember(Guid team_id, Guid guid)
{
	const auto team_entity = entt::to_entity(team_id);
	if (!tls.registry.valid(team_entity))
	{
		return kRetTeamHasNotTeamId;
	}
	auto* const try_team = tls.registry.try_get<Team>(team_entity);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	auto& members_ = try_team->members_;
	members_.erase(std::find(members_.begin(), members_.end(), guid));
	const auto pit = tlsCommonLogic.GetPlayerList().find(guid);
	if (pit == tlsCommonLogic.GetPlayerList().end())
	{
		return kRetTeamPlayerNotFound;
	}
	tls.registry.remove<TeamId>(pit->second);
	return kOK;
}