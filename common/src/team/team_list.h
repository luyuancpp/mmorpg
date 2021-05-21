#ifndef TEAM_LIST_H
#define  TEAM_LIST_H

#include <unordered_map>

#include "src/snow_flake/snow_flake.h"
#include "src/team/team_event.h"

#include "team.h"

namespace common
{
    class TeamList : public Receiver<TeamList>
    {
    public:
        using TeamPtr = std::shared_ptr<Team>;
        using TeamMap = std::unordered_map<GameGuid, TeamPtr>;
        using PlayerIdTeamIdMap = std::unordered_map<GameGuid, GameGuid>;
        using TeamIdList = std::unordered_set<GameGuid>;
        static const std::size_t kMaxTeamSize = 10000;

        TeamList();

        std::size_t team_size()const { return teams_.size(); }
        std::size_t member_size(GameGuid team_id);
        std::size_t applicant_size_by_player_id(GameGuid player_id)const;
        std::size_t applicant_size_by_team_id(GameGuid team_id)const;
        std::size_t players_size()const { return player_team_map_.size(); }
        const TeamMember& team_member(GameGuid player_id)const;
        GameGuid GetTeamId(GameGuid player_id)const;
        GameGuid last_team_id() const { return last_team_id_; }
        GameGuid leader_id_by_teamid(GameGuid team_id)const;
        GameGuid leader_id_by_player_id(GameGuid player_id)const;
        GameGuid first_applicant_id(GameGuid team_id)const;
       
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

        bool IsTeamsMax()const;
        bool IsTeamFull(GameGuid team_id);
        bool PlayerInTeam(GameGuid team_id, GameGuid player_id);
        bool PlayerInTeam(GameGuid player_id)const { return player_team_map_.find(player_id) != player_team_map_.end(); }
        bool FindTeamId(GameGuid team_id) const{ return teams_.find(team_id) != teams_.end(); }
        bool IsLeader(GameGuid team_id, GameGuid player_id);
        bool IsLeader(GameGuid player_id);
        bool FindTeamId(GameGuid player_id);
        bool HasApplicant(GameGuid team_id, GameGuid player_id)const;

        bool TestApplicantValueEqual(GameGuid team_id)const;
        
        void receive(const TeamEventStructJoinTeam& es);
        void receive(const TeamEventStructLeaderDismissTeam& es);
        void receive(const TeamEventStructLeaveTeam& es);
        void receive(const TeamEventStructDismissTeamOnTeamMemberEmpty& es);
        
    protected:

        ReturnValue JoinTeam(const Members& member_list, GameGuid  team_id);
        ReturnValue CheckMemberInTeam(const Members& member_list);
        void OnPlayerLeaveTeam(GameGuid player_id);
        void EraseTeam(GameGuid team_id);

        TeamMap teams_;
        PlayerIdTeamIdMap player_team_map_;
        SnowFlake snow_flake_;
        GameGuid last_team_id_{ 0 };
        EventManagerPtr emp_;
    };
}//namespace common



#endif // TEAM_LIST_H

