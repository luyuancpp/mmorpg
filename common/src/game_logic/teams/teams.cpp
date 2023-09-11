#include "teams.h"

#include "src/util/game_registry.h"
#include "src/game_logic/tips_id.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"

#include "component_proto/team_comp.pb.h"

Teams::~Teams()
{
	for (const auto& [fst, snd] : cl_tls.player_list())
	{
		LeaveTeam(fst);
	}
}

std::size_t Teams::member_size(const Guid team_id)
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

std::size_t Teams::applicant_size_by_player_id(const Guid guid) const
{
	const auto team_id = GetTeamId(guid);
	return applicant_size_by_team_id(team_id);
}

std::size_t Teams::applicant_size_by_team_id(const Guid team_id) const
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
	return try_team->applicant_size();
}

std::size_t Teams::players_size()
{
	return tls.registry.storage<TeamId>().size();
}

Guid Teams::GetTeamId(const Guid guid)
{
	auto pit = cl_tls.player_list().find(guid);
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

Guid Teams::get_leader_id_by_teamid(Guid team_id) const
{
	const auto team = entt::to_entity(team_id);
	if (!tls.registry.valid(team))
	{
		return kInvalidGuid;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team);
	if (nullptr == try_team)
	{
		return kInvalidGuid;
	}
	return try_team->leader_id();
}

Guid Teams::get_leader_id_by_player_id(Guid guid) const
{
	return get_leader_id_by_teamid(GetTeamId(guid));
}

Guid Teams::first_applicant(Guid team_id)
{
	const auto team = entt::to_entity(team_id);
	if (!tls.registry.valid(team))
	{
		return kInvalidGuid;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team);
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

bool Teams::IsTeamFull(Guid team_id)
{
	const auto team = entt::to_entity(team_id);
	if (!tls.registry.valid(team))
	{
		return false;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team);
	if (nullptr == try_team)
	{
		return false;
	}
	return try_team->IsFull();
}

bool Teams::HasMember(const Guid team_id, const Guid guid)
{
	const auto team = entt::to_entity(team_id);
	if (!tls.registry.valid(team))
	{
		return false;
	}
	const auto* const try_team = tls.registry.try_get<Team>(team);
	if (nullptr == try_team)
	{
		return false;
	}
	return try_team->HasMember(guid);
}

bool Teams::HasTeam(Guid guid)
{
	const auto pit = cl_tls.player_list().find(guid);
	if (pit == cl_tls.player_list().end())
	{
		return false;
	}
	return tls.registry.any_of<TeamId>(pit->second);
}

bool Teams::IsApplicant(Guid team_id, Guid guid)
{
	auto e = entt::to_entity(team_id);
	if (!tls.registry.valid(e))
	{
		return false;
	}
	const auto* const try_team = tls.registry.try_get<Team>(e);
	if (nullptr == try_team)
	{
		return false;
	}
	return try_team->IsApplicant(guid);
}

uint32_t Teams::CreateTeam(const CreateTeamP& param)
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
	const auto e = tls.registry.create();
	auto& team = tls.registry.emplace<Team>(e);
	team.leader_id_ = param.leader_id_;
	team.team_id_ = e;
	for (const auto& member_it : param.member_list)
	{
		team.AddMember(member_it);
	}
	last_team_id_ = entt::to_integral(e);
	return kRetOK;
}

uint32_t Teams::JoinTeam(Guid team_id, Guid guid)
{
	auto e = entt::to_entity(team_id);
	if (!tls.registry.valid(e))
	{
		return kRetTeamHasNotTeamId;
	}
	const auto try_team = tls.registry.try_get<Team>(e);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (Team::HasTeam(guid))
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

uint32_t Teams::JoinTeam(const UInt64Set& member_list, const Guid team_id)
{
	auto e = entt::to_entity(team_id);
	if (!tls.registry.valid(e))
	{
		return kRetTeamHasNotTeamId;
	}
	const auto* const try_team = tls.registry.try_get<Team>(e);
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

uint32_t Teams::CheckMemberInTeam(const UInt64Set& member_list) const
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

uint32_t Teams::LeaveTeam(Guid guid)
{
    auto team_id = GetTeamId(guid);
	auto e = entt::to_entity(team_id);
	if (!tls.registry.valid(e))
	{
		return kRetTeamHasNotTeamId;
	}
	auto try_team = tls.registry.try_get<Team>(e);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (!try_team->HasMember(guid))
	{
		return kRetTeamMemberNotInTeam;
	}
	bool is_leader_leave = try_team->IsLeader(guid);
	try_team->DelMember(guid);
	if (!try_team->members_.empty() && is_leader_leave)
	{
		try_team->OnAppointLeader(*try_team->members_.begin());
	}          
    if (try_team->empty())
    {
        EraseTeam(try_team->to_entityid());
    }
    return kRetOK;
}

uint32_t Teams::KickMember(Guid team_id, Guid current_leader, Guid be_kick_guid)
{
	auto e = entt::to_entity(team_id);
	if (!tls.registry.valid(e))
	{
		return kRetTeamHasNotTeamId;
	}
	auto try_team = tls.registry.try_get<Team>(e);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (try_team->leader_id_ != current_leader)
	{
		return kRetTeamKickNotLeader;
	}
	if (try_team->leader_id_ == be_kick_guid)
	{
		return kRetTeamKickSelf;
	}
	if (current_leader == be_kick_guid)
	{
		return kRetTeamKickSelf;
	}
	if (!try_team->HasMember(be_kick_guid))
	{
		return kRetTeamMemberNotInTeam;
	}
	try_team->DelMember(be_kick_guid);
    return kRetOK;
}

uint32_t Teams::Disbanded(Guid team_id, Guid current_leader_id)
{
	auto e = entt::to_entity(team_id);
	if (!tls.registry.valid(e))
	{
		return kRetTeamHasNotTeamId;
	}
	auto try_team = tls.registry.try_get<Team>(e);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (try_team->leader_id() != current_leader_id)
	{
		return kRetTeamDismissNotLeader;
	}
	const auto temp_member = try_team->members_;
	for (auto& it : temp_member)
	{
		try_team->DelMember(it);
	}
    EraseTeam(e);
    return kRetOK;
}

uint32_t Teams::DisbandedTeamNoLeader(Guid team_id)
{
	auto e = entt::to_entity(team_id);
	if (!tls.registry.valid(e))
	{
		return kRetTeamHasNotTeamId;
	}
	auto try_team = tls.registry.try_get<Team>(e);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
    return Disbanded(team_id, try_team->leader_id());
}

uint32_t Teams::AppointLeader(Guid team_id, Guid current_leader, Guid  new_leader)
{
	auto e = entt::to_entity(team_id);
	if (!tls.registry.valid(e))
	{
		return kRetTeamHasNotTeamId;
	}
	auto try_team = tls.registry.try_get<Team>(e);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (try_team->leader_id_ == new_leader)
	{
		return kRetTeamAppointSelf;
	}
	if (!try_team->HasMember(new_leader))
	{
		return kRetTeamHasNotTeamId;
	}
	if (try_team->leader_id_ != current_leader)
	{
		return kRetTeamAppointSelf;
	}
	try_team->OnAppointLeader(new_leader);
    return kRetOK;
}

uint32_t Teams::ApplyToTeam(Guid team_id, Guid guid)
{
	auto e = entt::to_entity(team_id);
	if (!tls.registry.valid(e))
	{
		return kRetTeamHasNotTeamId;
	}
	auto try_team = tls.registry.try_get<Team>(e);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	if (try_team->HasTeam(guid))
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

uint32_t Teams::DelApplicant(Guid team_id, Guid guid)
{
	auto e = entt::to_entity(team_id);
	if (!tls.registry.valid(e))
	{
		return kRetTeamHasNotTeamId;
	}
	auto try_team = tls.registry.try_get<Team>(e);
	if (nullptr == try_team)
	{
		return kRetTeamHasNotTeamId;
	}
	auto it = std::find(try_team->applicants_.begin(), try_team->applicants_.end(), guid);
	if (it != try_team->applicants_.end())
	{
		try_team->applicants_.erase(it);
	}
    return kRetOK;
}

void Teams::ClearApplyList(Guid team_id)
{
	auto e = entt::to_entity(team_id); 
	if (!tls.registry.valid(e))
	{
		return;
	}
	auto try_team = tls.registry.try_get<Team>(e);
	if (nullptr == try_team)
	{
		return;
	}
	try_team->applicants_.clear();
}

void Teams::EraseTeam(entt::entity team_id)
{
    tls.registry.destroy(team_id);
}

