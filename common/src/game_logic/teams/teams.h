#ifndef COMMON_SRC_GAME_LOGIC_TEAM_TEAMS_H_
#define COMMON_SRC_GAME_LOGIC_TEAM_TEAMS_H_

#include <unordered_map>

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"
#include "entt/src/entt/entity/entity.hpp"
#include "src/snow_flake/snow_flake.h"
#include "src/game_logic/teams/team_event.h"

#include "team.h"

namespace common
{
class Teams : public Receiver<Teams>
{
public:
    static const std::size_t kMaxTeamSize = 10000;

    Teams();

    std::size_t team_size()const { return reg().size<Team>(); }
    std::size_t member_size(Guid team_id);
    std::size_t applicant_size_by_guid(Guid guid)const;
    std::size_t applicant_size_by_team_id(Guid team_id)const;
    std::size_t players_size()const { return reg().get<PlayerTeamMap>(my_entity_id_).size(); }
    Guid GetTeamId(Guid guid)const;
    entt::entity GetTeamEntityId(Guid guid)const;
    Guid last_team_id() const { return last_team_id_; }
    Guid leader_id_by_teamid(Guid team_id)const;
    Guid leader_id_by_guid(Guid guid)const;
    Guid first_applicant(Guid team_id)const;
       
    bool IsTeamsMax() const{ return team_size() >= kMaxTeamSize; }
    bool IsTeamFull(Guid team_id);
    bool HasMember(Guid team_id, Guid guid);
    bool PlayerInTeam(Guid guid)const;
    bool HasApplicant(Guid team_id, Guid guid)const;
        
    uint32_t CreateTeam(const CreateTeamP& param);
    uint32_t JoinTeam(Guid team_id, Guid guid);
    uint32_t JoinTeam(const UInt64Set& member_list, Guid  team_id);
    uint32_t LeaveTeam(Guid guid);
    uint32_t KickMember(Guid team_id, Guid current_leader_id, Guid  kick_guid);
    uint32_t DissMissTeam(Guid team_id, Guid current_leader_id);
    uint32_t DissMissTeamNoLeader(Guid team_id);
    uint32_t AppointLeader(Guid team_id, Guid current_leader_id, Guid  nNewLeaderPlayerId);
    uint32_t ApplyForTeam(Guid team_id, Guid guid);
    uint32_t RejectApplicant(Guid team_id, Guid apply_guid);
    uint32_t AgreeApplicant(Guid team_id, Guid apply_guid);
    void ClearApplyList(Guid team_id);
        
private:
    uint32_t CheckMemberInTeam(const UInt64Set& member_list);
    void EraseTeam(entt::entity team_id);

    Guid last_team_id_{ 0 };
    EventManagerPtr emp_;
    entt::entity my_entity_id_{};
};
}//namespace common
#endif // COMMON_SRC_GAME_LOGIC_TEAM_TEAMS_H_

