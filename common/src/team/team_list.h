#ifndef TEAM_LIST_H
#define  TEAM_LIST_H

#include <unordered_map>

#include "src/snow_flake/snow_flake.h"
#include "Team.h"

namespace common
{
    class TeamList
    {
    public:
        using TeamPtr = std::shared_ptr<Team>;
        using TeamMap = std::unordered_map<GameGuid, TeamPtr>;
        using PlayerIdTeamIdMap = std::unordered_map<GameGuid, GameGuid>;
        using TeamIdList = std::unordered_set<GameGuid>;
        static const std::size_t kMaxTeamSize = 10000;

        std::size_t team_size()const { return teams_.size(); }
        std::size_t member_size(GameGuid team_id);
        const TeamMember& team_member(GameGuid player_id);

        ReturnValue CreateTeam(const CreateTeamParam& param);
        ReturnValue JoinTeam(GameGuid team_id, TeamMember& mem);
        ReturnValue LeaveTeam(GameGuid team_id, GameGuid player_id);
        ReturnValue KickMember(GameGuid team_id, GameGuid current_leader_id, GameGuid  kick_player_id);
        ReturnValue DissMissTeam(GameGuid team_id, GameGuid current_leader_id);
        ReturnValue DissMissTeamNoLeader(GameGuid team_id);
        ReturnValue AppointLeader(GameGuid team_id, GameGuid current_leader_id, GameGuid  nNewLeaderPlayerId);
        ReturnValue Apply(GameGuid team_id, const TeamMember& m);
        ReturnValue RemoveApplicant(GameGuid team_id, GameGuid apply_player_id);
        ReturnValue AgreeApplicant(GameGuid team_id, GameGuid apply_player_id);
        void ClearApplyList(GameGuid team_id);

        bool IsTeamListFull()const;
        bool IsTeamFull(GameGuid team_id);
        bool InTeam(GameGuid team_id, GameGuid player_id);
        bool FindTeamId(GameGuid team_id) const{ return teams_.find(team_id) != teams_.end(); }
        bool PlayerInTeam(GameGuid player_id)const { return player_team_map_.find(player_id) != player_team_map_.end(); }
        bool IsLeader(GameGuid team_id, GameGuid player_id);
        bool IsLeader(GameGuid player_id);
        bool FindTeamId(GameGuid player_id);
        GameGuid GetTeamId(GameGuid player_id);
        
#ifdef __TEST__
        Team* GetTeam(GameGuid team_id)
        {
            TeamMap::iterator it = teams_.find(team_id);
            if (it == teams_.end())
            {
                return NULL;
            }
            return it->second.get();
        }
#endif // __TEST__
    protected:

        ReturnValue TryToJoinTheTeam(const Members& member_list,
            GameGuid  team_id);

        ReturnValue JoinTeam(const Members& member_list,
            GameGuid  team_id);

        void EraseTeam(GameGuid team_id);

    protected:
        TeamMap teams_;
        PlayerIdTeamIdMap player_team_map_;
        SnowFlake snow_flake_;
    };
}//namespace common



#endif // TEAM_LIST_H

