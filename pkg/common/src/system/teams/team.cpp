#include "team.h"

#include "src/thread_local/thread_local_storage_common_logic.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/constants/tips_id.h"

#include "component_proto/team_comp.pb.h"

void Team::OnAppointLeader(const Guid new_leader_guid)
{
    leader_id_ = new_leader_guid;
}

void Team::AddMember(Guid guid)
{
    const auto pit = cl_tls.player_list().find(guid);
    if (pit == cl_tls.player_list().end())
    {
        return;
    }
    members_.emplace_back(guid);
    tls.registry.emplace<TeamId>(pit->second).set_team_id(entt::to_integral(team_id_));
}

void Team::DelMember(Guid guid)
{
    members_.erase(std::find(members_.begin(), members_.end(), guid));
    const auto pit = cl_tls.player_list().find(guid);
    if (pit == cl_tls.player_list().end())
    {
        return;
    }
    tls.registry.remove<TeamId>(pit->second);
}
