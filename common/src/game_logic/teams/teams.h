#ifndef COMMON_SRC_GAME_LOGIC_TEAM_TEAMS_H_
#define COMMON_SRC_GAME_LOGIC_TEAM_TEAMS_H_

#include <unordered_map>

#include "src/common_type/common_type.h"
#include "entt/src/entt/entity/registry.hpp"
#include "entt/src/entt/entity/entity.hpp"
#include "src/snow_flake/snow_flake.h"
#include "src/game_logic/teams/team_event.h"

#include "team.h"

namespace common
{
    class Teams : public Receiver<Teams>
    {
    public:
        using PlayerIdTeamIdMap = std::unordered_map<GameGuid, entt::entity>;
        static const std::size_t kMaxTeamSize = 10000;

        Teams();

        std::size_t team_size()const { return teams_.size<Team>(); }
        std::size_t member_size(GameGuid team_id);
        std::size_t applicant_size_by_player_id(GameGuid player_id)const;
        std::size_t applicant_size_by_team_id(GameGuid team_id)const;
        std::size_t players_size()const { return player_team_map_.size(); }
        const TeamMember& team_member(GameGuid player_id)const;
        GameGuid GetTeamId(GameGuid player_id)const;
        entt::entity GetTeamEntityId(GameGuid player_id)const;
        GameGuid last_team_id() const { return last_team_id_; }
        GameGuid leader_id_by_teamid(GameGuid team_id)const;
        GameGuid leader_id_by_player_id(GameGuid player_id)const;
        GameGuid first_applicant_id(GameGuid team_id)const;
       
        bool IsTeamsMax() const{ return team_size() >= kMaxTeamSize; }
        bool IsTeamFull(GameGuid team_id);
        bool PlayerInTeam(GameGuid team_id, GameGuid player_id);
        bool PlayerInTeam(GameGuid player_id)const { return player_team_map_.find(player_id) != player_team_map_.end(); }
        bool FindTeamId(GameGuid team_id) const{ return nullptr !=  teams_.try_get<Team>(entt::to_entity(team_id)); }
        bool IsLeader(GameGuid team_id, GameGuid player_id);
        bool IsLeader(GameGuid player_id);
        bool FindTeamId(GameGuid player_id);
        bool HasApplicant(GameGuid team_id, GameGuid player_id)const;

        bool TestApplicantValueEqual(GameGuid team_id)const;
        
        void receive(const TeamESJoinTeam& es);
        void receive(const TeamESCreateTeamJoinTeam& es);        
        void receive(const TeamESLeaderDismissTeam& es);
        void receive(const TeamESLeaveTeam& es);

        ReturnValue CreateTeam(const CreateTeamParam& param);
        ReturnValue JoinTeam(GameGuid team_id, TeamMember& mem);
        ReturnValue LeaveTeam(GameGuid player_id);
        ReturnValue KickMember(GameGuid team_id, GameGuid current_leader_id, GameGuid  kick_player_id);
        ReturnValue DissMissTeam(GameGuid team_id, GameGuid current_leader_id);
        ReturnValue DissMissTeamNoLeader(GameGuid team_id);
        ReturnValue AppointLeader(GameGuid team_id, GameGuid current_leader_id, GameGuid  nNewLeaderPlayerId);
        ReturnValue ApplyForTeam(GameGuid team_id, const TeamMember& m);
        ReturnValue RemoveApplicant(GameGuid team_id, GameGuid apply_player_id);
        ReturnValue AgreeApplicant(GameGuid team_id, GameGuid apply_player_id);
        void ClearApplyList(GameGuid team_id);
        
    protected:
        ReturnValue JoinTeam(const Members& member_list, GameGuid  team_id);
        ReturnValue CheckMemberInTeam(const Members& member_list);
        void OnPlayerLeaveTeam(GameGuid player_id);
        void EraseTeam(entt::entity team_id);
        void OnJoinTeam(GameGuid player_id, entt::entity team_id)
        {
            player_team_map_.emplace(player_id, team_id);
        }

        PlayerIdTeamIdMap player_team_map_;
        GameGuid last_team_id_{ 0 };
        EventManagerPtr emp_;
        entt::registry teams_;
    };
}//namespace common
#endif // COMMON_SRC_GAME_LOGIC_TEAM_TEAMS_H_

