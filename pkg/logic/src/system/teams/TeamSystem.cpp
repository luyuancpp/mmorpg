#include "TeamSystem.h"

#include "src/thread_local/thread_local_storage_common_logic.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/constants/tips_id.h"
#include "src/util/game_registry.h"

#include "component_proto/team_comp.pb.h"

TeamSystem::~TeamSystem()
{
	for (const auto& [fst, snd] : cl_tls.player_list())
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
	const auto pit = cl_tls.player_list().find(guid);
	if (pit == cl_tls.player_list().end())
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
	const auto pit = cl_tls.player_list().find(guid);
	if (pit == cl_tls.player_list().end())
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
	RET_CHECK_RET(CheckMemberInTeam(param.member_list))
	const auto team_entity = tls.registry.create();
	auto& team = tls.registry.emplace<Team>(team_entity);
	team.leader_id_ = param.leader_id_;
	team.team_id_ = team_entity;
	for (const auto& member_it : param.member_list)
	{
		team.AddMember(member_it);
	}
	last_team_id_ = entt::to_integral(team_entity);
	return kRetOK;
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
	try_team->AddMember(guid);
	return kRetOK;
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

	RET_CHECK_RET(CheckMemberInTeam(member_list))
	for (const auto& member_it : member_list)
	{
		RET_CHECK_RET(JoinTeam(team_id, member_it))
	}
	return kRetOK;
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
	return kRetOK;
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
	try_team->DelMember(guid);
	if (!try_team->members_.empty() && is_leader_leave)
	{
		try_team->OnAppointLeader(*try_team->members_.begin());
	}
	if (try_team->empty())
	{
		EraseTeam(try_team->to_entity_id());
	}
	return kRetOK;
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
	try_team->DelMember(be_kick_id);
	return kRetOK;
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
		try_team->DelMember(member_it);
	}
	EraseTeam(team_entity);
	return kRetOK;
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
	return kRetOK;
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
	return kRetOK;
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
	return kRetOK;
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

