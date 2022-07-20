#include "teams.h"

#include "src/game_logic/tips_id.h"

#include "src/game_logic/player/player_list.h"

#include "component_proto/team_comp.pb.h"

#define GetTeamPtrReturnError \
auto e = entt::to_entity(team_id);\
if (!registry.valid(e))\
{\
    return kRetTeamHasNotTeamId;\
}\
auto try_team = registry.try_get<Team>(e);\
if (nullptr == try_team)\
{\
    return kRetTeamHasNotTeamId;\
}\
auto& team = *try_team;\

#define GetTeamEntityReturnError \
if (!registry.valid(team_id))\
{\
    return kRetTeamHasNotTeamId;\
}\
auto try_team = registry.try_get<Team>(e);\
if (nullptr == try_team)\
{\
    return kRetTeamHasNotTeamId;\
}\
auto& team = *try_team;\

#define GetTeamReturn(ret) \
auto e = entt::to_entity(team_id);\
if (!registry.valid(e))\
{\
    return ret;\
}\
auto try_team = registry.try_get<Team>(e);\
if (nullptr == try_team)\
{\
    return ret;\
}\
auto& team = *try_team;\

#define GetTeamReturnVoid \
auto e = entt::to_entity(team_id);\
if (!registry.valid(e))\
{\
    return ;\
}\
auto try_team = registry.try_get<Team>(e);\
if (nullptr == try_team)\
{\
    return;\
}\
auto& team = *try_team;\

Teams::Teams()
{
    my_entity_id_ = registry.create();
}

Teams::~Teams()
{
    for (auto it : *g_players)
    {
        LeaveTeam(it.first);
    }
}

std::size_t Teams::member_size(Guid team_id)
{
    GetTeamReturn(0);
    return team.member_size();
}

std::size_t Teams::applicant_size_by_playerid(Guid guid) const
{
    auto team_id = GetTeamId(guid);
    return applicant_size_by_team_id(team_id);
}

std::size_t Teams::applicant_size_by_team_id(Guid team_id) const
{
    GetTeamReturn(0);
    return team.applicant_size();
}

std::size_t Teams::players_size()const
{
    return registry.storage<TeamId>().size();
}

Guid Teams::GetTeamId(Guid guid)const
{
    auto pit = g_players->find(guid);
    if (pit == g_players->end())
    {
        return entt::null_t();
    }
    auto try_team_id =  registry.try_get<TeamId>(pit->second);
    if (nullptr == try_team_id)
    {
        return entt::null_t();
    }
    return try_team_id->team_id();
}

Guid Teams::get_leader_id_by_teamid(Guid team_id) const
{
    GetTeamReturn(kInvalidGuid);
    return team.leader_id();
}

Guid Teams::get_leader_id_by_playerid(Guid guid) const
{
    auto team_id = GetTeamId(guid);
    return get_leader_id_by_teamid(team_id);
}
    
Guid Teams::first_applicant(Guid team_id) const
{
    GetTeamReturn(0);
    return team.first_applicant();
}

bool Teams::IsTeamFull(Guid team_id)
{
    GetTeamReturn(false);
    return team.IsFull();
}

bool Teams::HasMember(Guid team_id, Guid guid)
{
    GetTeamReturn(false);
    return team.IsMember(guid);
}

bool Teams::HasTeam(Guid guid) const
{
    auto pit = g_players->find(guid);
    if (pit == g_players->end())
    {
        return false;
    }
    return registry.any_of<TeamId>(pit->second);
}

bool Teams::IsApplicant(Guid team_id, Guid guid) const
{
    GetTeamReturn(false);
    return team.IsApplicant(guid);
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
    RET_CHECK_RET(CheckMemberInTeam(param.members));
    auto e = registry.create();
    registry.emplace<Team>(e, param, e);
    last_team_id_ = entt::to_integral(e);//for test
    return kRetOK;
}

uint32_t Teams::JoinTeam(Guid team_id, Guid guid)
{
    GetTeamPtrReturnError;
    return team.JoinTeam(guid);
}

uint32_t Teams::JoinTeam(const UInt64Set& member_list, Guid  team_id)
{
    GetTeamPtrReturnError;
    RET_CHECK_RET(CheckMemberInTeam(member_list));
    for (auto& it : member_list)
    {
        RET_CHECK_RET(team.JoinTeam(it));
    }
    return kRetOK;
}

uint32_t Teams::CheckMemberInTeam(const UInt64Set& member_list)
{
    for (auto& it : member_list)
    {
        if (HasTeam(it))
        {
            return kRetTeamMemberInTeam;
        }
    }
    return kRetOK;
}

uint32_t Teams::LeaveTeam(Guid guid)
{
    auto team_id = GetTeamId(guid);
    GetTeamPtrReturnError;
    RET_CHECK_RET(team.LeaveTeam(guid));
    if (team.empty())
    {
        EraseTeam(team.to_entityid());
    }
    return kRetOK;
}

uint32_t Teams::KickMember(Guid team_id, Guid current_leader, Guid  kick_guid)
{
    GetTeamPtrReturnError;
    RET_CHECK_RET(team.KickMember(current_leader, kick_guid));
    return kRetOK;
}

uint32_t Teams::DissMissTeam(Guid team_id, Guid current_leader)
{
    GetTeamPtrReturnError;
    RET_CHECK_RET(team.DissMiss(current_leader));
    EraseTeam(e);
    return kRetOK;
}

uint32_t Teams::DissMissTeamNoLeader(Guid team_id)
{
    GetTeamPtrReturnError;
    return DissMissTeam(team_id, team.leader_id());
}

uint32_t Teams::AppointLeader(Guid team_id, Guid current_leader, Guid  new_leader)
{
    GetTeamPtrReturnError;
    return team.AppointLeader(current_leader, new_leader);
}

uint32_t Teams::ApplyToTeam(Guid team_id, Guid guid)
{
    GetTeamPtrReturnError;
    return team.ApplyToTeam(guid);
}

uint32_t Teams::DelApplicant(Guid team_id, Guid guid)
{
    GetTeamPtrReturnError;
    return team.DelApplicant(guid);
}

void Teams::ClearApplyList(Guid team_id)
{
    GetTeamReturnVoid;
    team.ClearApplyList();
}

void Teams::EraseTeam(entt::entity team_id)
{
    registry.destroy(team_id);
}

