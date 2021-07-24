#include "teams.h"

#include "src/return_code/return_notice_code.h"

namespace common
{
#define GetTeamPtrReturnError \
    auto e = entt::to_entity(team_id);\
    if (!teams_registry_.valid(e))\
    {\
        return RET_TEAM_HAS_NOT_TEAM_ID;\
    }\
    auto& team = teams_registry_.get<Team>(e);\

#define GetTeamEntityReturnError \
    if (!teams_registry_.valid(team_id))\
    {\
        return RET_TEAM_HAS_NOT_TEAM_ID;\
    }\
    auto& team = teams_registry_.get<Team>(team_id);\

#define GetTeamReturn(ret) \
    auto e = entt::to_entity(team_id);\
    if (!teams_registry_.valid(e))\
    {\
        return ret;\
    }\
    auto& team = teams_registry_.get<Team>(e);\

    Teams::Teams()
        : emp_(EventManager::New())
    {
        my_entity_id_ = teams_registry_.create();
        teams_registry_.emplace<PlayerIdTeamIdMap>(my_entity_id_, PlayerIdTeamIdMap());
    }

    std::size_t Teams::member_size(GameGuid team_id)
    {
        GetTeamReturn(0);
        return team.member_size();
    }

    std::size_t Teams::applicant_size_by_player_id(GameGuid player_id) const
    {
        auto team_id = GetTeamId(player_id);
        return applicant_size_by_team_id(team_id);
    }

    std::size_t Teams::applicant_size_by_team_id(GameGuid team_id) const
    {
        GetTeamReturn(0);
        return team.applicant_size();
    }

    GameGuid Teams::GetTeamId(GameGuid player_id)const
    {
        auto& player_team_map_ = teams_registry_.get<PlayerIdTeamIdMap>(my_entity_id_);
        auto it = player_team_map_.find(player_id);
        if (it == player_team_map_.end())
        {
            return kEmptyGameGuid;
        }
        return entt::to_integral(it->second);
    }

    entt::entity Teams::GetTeamEntityId(GameGuid player_id) const
    {
        auto& player_team_map_ = teams_registry_.get<PlayerIdTeamIdMap>(my_entity_id_);
        auto it = player_team_map_.find(player_id);
        if (it == player_team_map_.end())
        {
            return entt::null;
        }
        return it->second;
    }

    common::GameGuid Teams::leader_id_by_teamid(GameGuid team_id) const
    {
        GetTeamReturn(kEmptyGameGuid);
        return team.leader_id();
    }

    common::GameGuid Teams::leader_id_by_player_id(GameGuid player_id) const
    {
        auto team_id = GetTeamId(player_id);
        return leader_id_by_teamid(team_id);
    }
    
    common::GameGuid Teams::first_applicant_id(GameGuid team_id) const
    {
        GetTeamReturn(0);
        return team.first_applicant_id();
    }
    bool Teams::IsTeamFull(GameGuid team_id)
    {
        GetTeamReturn(false);
        return team.IsFull();
    }

    bool Teams::PlayerInTheTeam(GameGuid team_id, GameGuid player_id)
    {
        GetTeamReturn(false);
        return team.InTeam(player_id);
    }

    bool Teams::PlayerInTeam(GameGuid player_id) const
    {
        auto& player_team_map_ = teams_registry_.get<PlayerIdTeamIdMap>(my_entity_id_);
        return player_team_map_.find(player_id) != player_team_map_.end(); 
    }

    bool Teams::HasApplicant(GameGuid team_id, GameGuid player_id) const
    {
        GetTeamReturn(false);
        return team.HasApplicant(player_id);
    }

    bool Teams::TestApplicantValueEqual(GameGuid team_id)const
    {
        GetTeamReturn(false);
        return team.TestApplicantValueEqual();
    }

    uint32_t Teams::CreateTeam(const CreateTeamParam& param)
    {
        if (IsTeamsMax())
        {
            return RET_TEAM_TEAM_LIST_MAX;
        }
        if (PlayerInTeam(param.leader_id_))
        {
            return RET_TEAM_MEMBER_IN_TEAM;
        }
        RET_CHECK_RET(CheckMemberInTeam(param.members));

        auto e = teams_registry_.create();
        TeamsParam ts_param{e, my_entity_id_, emp_, &teams_registry_ };
        auto team = teams_registry_.emplace<Team>(e, param, ts_param);

        PlayerInTeamF f_in_the_team;
        f_in_the_team.cb_ = std::bind(&Teams::PlayerInTeam, this, std::placeholders::_1);
        teams_registry_.emplace<PlayerInTeamF>(e, f_in_the_team);

        team.OnCreate();
        last_team_id_ = entt::to_integral(e);
        return RET_OK;
    }

    uint32_t Teams::JoinTeam(GameGuid team_id, GameGuid player_id)
    {
        GetTeamPtrReturnError;
        return team.JoinTeam(player_id);
    }

    uint32_t Teams::CheckMemberInTeam(const UI64USet& member_list)
    {
        for (auto& it : member_list)
        {
            if (PlayerInTeam(it))
            {
                return RET_TEAM_MEMBER_IN_TEAM;
            }
        }
        return RET_OK;
    }

    uint32_t Teams::LeaveTeam(GameGuid player_id)
    {
        auto team_id = GetTeamEntityId(player_id);
        GetTeamEntityReturnError;
        RET_CHECK_RET(team.LeaveTeam(player_id));
        if (team.empty())
        {
            EraseTeam(team.to_entityid());
        }
        return RET_OK;
    }

    uint32_t Teams::KickMember(GameGuid team_id, GameGuid current_leader_id, GameGuid  kick_player_id)
    {
        GetTeamPtrReturnError;
        RET_CHECK_RET(team.KickMember(current_leader_id, kick_player_id));
        return RET_OK;
    }

    uint32_t Teams::DissMissTeam(GameGuid team_id, GameGuid current_leader_id)
    {
        GetTeamPtrReturnError;
        RET_CHECK_RET(team.DissMiss(current_leader_id));
        EraseTeam(e);
        return RET_OK;
    }

    uint32_t Teams::DissMissTeamNoLeader(GameGuid team_id)
    {
        GetTeamPtrReturnError;
        return DissMissTeam(team_id, team.leader_id());
    }

    uint32_t Teams::AppointLeader(GameGuid team_id, GameGuid current_leader_id, GameGuid  new_leader_player_id)
    {
        GetTeamPtrReturnError;
        return team.AppointLeader(current_leader_id, new_leader_player_id);
    }

    uint32_t Teams::ApplyForTeam(GameGuid team_id, GameGuid player_id)
    {
        GetTeamPtrReturnError;
        return team.ApplyForTeam(player_id);
    }

    uint32_t Teams::RemoveApplicant(GameGuid team_id, GameGuid nApplyplayer_id)
    {
        GetTeamPtrReturnError;
        return team.RemoveApplicant(nApplyplayer_id);
    }

    uint32_t Teams::AgreeApplicant(GameGuid team_id, GameGuid applicant_id)
    {
        GetTeamPtrReturnError;
        return team.AgreeApplicant(applicant_id);
    }

    void Teams::ClearApplyList(GameGuid team_id)
    {
        auto e = entt::to_entity(team_id);
        if (!teams_registry_.valid(e))
        {
            return;
        }
        auto& team = teams_registry_.get<Team>(e);
        team.ClearApplyList();
    }

    uint32_t Teams::JoinTeam(const UI64USet& member_list, GameGuid  team_id)
    {
        GetTeamPtrReturnError;
        RET_CHECK_RET(CheckMemberInTeam(member_list));
        for (auto& it : member_list)
        {
            RET_CHECK_RET(team.JoinTeam(it));
        }
        return RET_OK;
    }

    void Teams::EraseTeam(entt::entity team_id)
    {
        teams_registry_.destroy(team_id);
    }

}//namespace common

