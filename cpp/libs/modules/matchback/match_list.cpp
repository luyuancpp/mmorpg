#include "MatchMaking.h"

#include <algorithm>

#include "GenCode/GameDefine_Result.h"
#include "GenCode/Match/MatchModule.h"
#include "CommonLogic/EventStruct/TeamEventStruct.h"
#include "GenCode/Config/MatchCfg.h"

#include "ModuleMatch.pb.h"

namespace GameMMR
{
    static const MatchRoom::match_cb_type kEmptyRematchCallback;
    static const MatchRoom::remove_unit_cb_type  kEmptyRemoveUnitCallback;
    MatchMaking::EventCallback kEmptyMakingEventCallback;

    MatchMaking::MatchMaking(int32_t matchType, TeamList * oTeamList)
        : m_bWithoutRepetitionProfession(false),
        m_nType(matchType),
        m_pTeamList(oTeamList)
    {
        m_pTeamList->emp()->subscribe<TeamEventES>(*this);
        m_pMatchElement = MatchTable::Instance().GetElement(m_nType);
        if (NULL != m_pMatchElement && m_pMatchElement->per_team_member_size > 0)
        {
            m_nkPerTeamMemberMaxSize = (std::size_t)m_pMatchElement->per_team_member_size;
        }

        if (NULL != m_pMatchElement && m_pMatchElement->per_camp_player_size > 0)
        {
            m_nkPerCampPlayerSize = (std::size_t)m_pMatchElement->per_camp_player_size;
        }


        if (NULL != m_pMatchElement && m_pMatchElement->per_scene_camp_size > 0)
        {
            m_nkMaxSceneCampSize = (std::size_t)m_pMatchElement->per_scene_camp_size;
        }

        if (NULL != m_pMatchElement && m_pMatchElement->profession_size > 0)
        {
            m_nkMaxProfessionSize = (std::size_t)m_pMatchElement->profession_size;
        }



    }

    int32_t MatchMaking::Match(match_unit_type & mp)
    {
        OR_CHECK_RESULT(CheckCanMatchMemberSize(mp));
        if (!mp->HasRobot() && InMatch(mp))
        {
            return OR_MATCH_IN_MATCH_ERROR;
        }
        OR_CHECK_RESULT(mp->CanMatch());
        OR_CHECK_RESULT(OnPlayerMatch(mp));
        UpdateMatchQueue(mp->MatchEloId());

        return OR_OK;
    }


    bool MatchMaking::InMatch(match_unit_type & p)
    {
        for (auto && it : p->GetPlayers())
        {
            if (m_vPlayers.find(it.first) != m_vPlayers.end())
            {
                return true;
            }
        }
        return false;
    }

    void MatchMaking::SetEnterDungeonCallback(enter_dungeon_callback o)
    {
        if (nullptr == m_pMatchElement)
        {
            return;
        }

        if (m_pMatchElement->is_dugeon <= 0)
        {
            return;
        }

        m_oEnterDungeonCallBack = o;
    }

    bool MatchMaking::IsRank()const
    {
        if (NULL == m_pMatchElement)
        {
            return false;
        }
        return m_pMatchElement->is_rank == 1;
    }

    bool MatchMaking::InGroupMatch(GUID_t  playerguid)
    {

        palyers_type::iterator it = m_vPlayers.find(playerguid);
        if (it == m_vPlayers.end())
        {
            return false;
        }

        group_queue_type::iterator et = m_GroupPlayer.find(it->second.m_nEloId);
        if (et == m_GroupPlayer.end())
        {
            return false;
        }

        match_queue_type::iterator pi = et->second.find(it->second.m_nMatchUnitId);
        if (pi == et->second.end())
        {
            return false;
        }

        if (pi->second->HasPlayer(playerguid))
        {
            return true;
        }

        return false;
    }

    bool MatchMaking::InMatch(GUID_t  playerguid)
    {
#ifdef __TEST__

        if (m_vPlayers.find(playerguid) != m_vPlayers.end())
        {
            return InGroupTestMatch(playerguid) || InScene(playerguid);
        }
        else
        {
            return false;
        }
        return  true;
#endif // __TEST__

        return m_vPlayers.find(playerguid) != m_vPlayers.end();
    }


    bool MatchMaking::InGroupTestMatch(GUID_t  playerguid)
    {
        bool groupRet = false;
        for (group_queue_type::iterator it = m_GroupPlayer.begin(); it != m_GroupPlayer.end(); ++it)
        {

            for (match_queue_type::iterator ji = it->second.begin(); ji != it->second.end(); ++ji)
            {
                if (ji->second->HasPlayer(playerguid))
                {
                    groupRet = true;
                    break;
                }
            }
            if (groupRet)
            {
                break;
            }
        }
        return groupRet;

    }

    bool MatchMaking::InScene(GUID_t  playerguid)
    {
        bool sceneRet = false;
        for (scenee_list_type::iterator it = m_vScenes.begin(); it != m_vScenes.end(); ++it)
        {
            if (it->second->HasPlayer(playerguid))
            {
                sceneRet = true;
                break;
            }
        }
        return sceneRet;
    }

    int32_t MatchMaking::CancelMatch(GUID_t  playerguid, int32_t nCancelSelfMatchUnitTeam)
    {
        palyers_type::iterator allPlayerIt = m_vPlayers.find(playerguid);
        if (allPlayerIt == m_vPlayers.end())
        {
            return OR_MATCH_PLAYER_NOT_IN_MATCH;
        }

        group_queue_type::iterator groupIt = m_GroupPlayer.find(allPlayerIt->second.m_nEloId);
        if (groupIt == m_GroupPlayer.end())
        {
            return OR_MATCH_ELO_MATCH__ERROR;
        }
        uint64_t matchUnitId = allPlayerIt->second.m_nMatchUnitId;
        match_queue_type::iterator matchUnitIt = groupIt->second.find(allPlayerIt->second.m_nMatchUnitId);
        if (matchUnitIt == groupIt->second.end())
        {
            OR_CHECK_RESULT(SceneCancel(GetSceneId(playerguid), playerguid));
        }

        if (matchUnitIt == groupIt->second.end())
        {
            return OR_MATCH_PLAYER_NOT_IN_MATCH;
        }
        if (nCancelSelfMatchUnitTeam == 0)
        {
            matchUnitIt->second->PlayerCancel(playerguid);
            SencCancelMatch(playerguid);
          
            m_vPlayers.erase(playerguid);
            if (matchUnitIt->second->IsEmpty())
            {
                groupIt->second.erase(matchUnitId);
            }
        }
        else
        {
            if (matchUnitIt != groupIt->second.end())
            {
                if (m_oSendCallBack)
                {
                    for (auto & unitPlayerIt : matchUnitIt->second->GetPlayers())
                    {
                        SencCancelMatch(unitPlayerIt.first);
                    }
                }
                
                for (auto & unitPlayerIt : matchUnitIt->second->GetPlayers())
                {
                    m_vPlayers.erase(unitPlayerIt.first);
                }
            }
            else
            {
                m_vPlayers.erase(playerguid);
            }
           
            groupIt->second.erase(matchUnitId);
        }

        return OR_OK;
    }

    int32_t MatchMaking::MatchSuccess(GUID_t  playerguid)
    {
        palyers_type::iterator it = m_vPlayers.find(playerguid);
        if (it == m_vPlayers.end())
        {
            return OR_MATCH_PLAYER_NOT_IN_MATCH;
        }
        group_queue_type::iterator et = m_GroupPlayer.find(it->second.m_nEloId);
        if (et == m_GroupPlayer.end())
        {
            return OR_MATCH_ELO_MATCH__ERROR;
        }
        et->second.erase(it->second.m_nMatchUnitId);
        return OR_OK;
    }

    void MatchMaking::GetGroupSortQueue(int32_t  eloValue, sort_match_map_type & mv)
    {
        group_queue_type::iterator it = m_GroupPlayer.find(eloValue);
        sort_match_vec_type v;
        if (it == m_GroupPlayer.end())
        {
            return;
        }
        match_queue_type & g = it->second;
        for (auto && mi : g)
        {
            v.push_back(mi.second);
        }
        std::stable_sort(v.begin(), v.end());

        int32_t i = 0;
        for (auto && si : v)
        {
            mv.emplace(i++, si);
        }
        return;
    }

    int32_t MatchMaking::OnPlayerMatch(match_unit_type & p)
    {
      
        group_queue_type::iterator gIt = m_GroupPlayer.find(p->MatchEloId());
        if (gIt == m_GroupPlayer.end())
        {
            match_queue_type v;
            std::pair<group_queue_type::iterator, bool> ret = m_GroupPlayer.emplace(p->MatchEloId(), v);
            if (!ret.second)
            {
                return OR_MATCH_UNKOWN_ERROR;
            }
            gIt = ret.first;
        }
        if (gIt == m_GroupPlayer.end())
        {
            return OR_MATCH_UNKOWN_ERROR;
        }
        gIt->second.emplace(p->GetUnitId(), p);

        for (auto && it : p->GetPlayers())
        {
            PrivePlayerInfo pp;
            pp.m_nMatchUnitId = p->GetUnitId();
            pp.m_nEloId = p->MatchEloId();
            auto pit = m_vPlayers.find(it.first);
            if (pit == m_vPlayers.end())
            {
                m_vPlayers.emplace(it.first, pp);
            }
            else
            {
                pit->second = pp;
            }
            SendMatchToClient(it.first);
        }

        return OR_OK;
    }

    bool MatchMaking::UpdateMatchRankQueue(int32_t eloValue)
    {
        sort_match_map_type players;
        GetGroupSortQueue(eloValue, players);
        if (players.empty())
        {
            return true;
        }
        while (!players.empty())
        {
            UpdateMatchRankQueue(eloValue, players);
            if (!players.empty())
            {
                players.erase(players.begin());
            }
        }

        return true;
    }

    bool MatchMaking::UpdateMatchRankQueue(int32_t eloValue, sort_match_map_type & players)
    {
        std::size_t matchsize = 0;

        scene_camps_vec_type scenecamps(GetMaxSceneCampSize()); // e.g. 15v15: 2 camps
        std::size_t nTeamSize = GetPerCampPlayerSize() / GetPerTeamMemberMaxSize(); // e.g. 15v15: 3 teams

        match_queue_type successList;
        i32_v_type  vEraseIndex;
        for (auto && it : scenecamps)
        {
            for (std::size_t i = 0; i < nTeamSize; ++i)
            {
                std::size_t ms = 0;
                match_queue_type matched = MatchTeamQueue(players, successList, ms, vEraseIndex);

                if (ms < GetPerTeamMemberMaxSize())
                {
                    return false;
                }

                if (matched.empty())
                {
                    return false;
                }

                for (auto & mi : matched)
                {
                    successList.emplace(mi.first, mi.second);
                }
                for (auto & ei : vEraseIndex)
                {
                    players.erase(ei);
                }
                it.push_back(matched);
                matchsize += ms;
            }
        }

        if (matchsize < GetSceneMaxPlayerSize())
        {
            return false;
        }

        MatchTargetInfo target;
        CreateMatchRoomParam cp
        {
            nullptr,
            scenecamps,
            eloValue,
            m_nType,
            std::bind(&MatchMaking::ReMatch, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
            std::bind(&MatchMaking::RemoveMatchUnit, this, std::placeholders::_1),
            m_pTeamList,
            &target,
            m_oSendCallBack,
            m_oSendGameCallBack
        };

      
        scene_type r(new MatchRoom(cp));


        AddScene(r);

        for (auto && sIt : successList)
        {
            for (auto && pIt : sIt.second->GetPlayers())
            {
                MatchSuccess(pIt.first);
                palyers_type::iterator ji = m_vPlayers.find(pIt.first);
                if (ji == m_vPlayers.end())
                {
                    continue;
                }
                ji->second.m_nSceneId = r->GetSceneId();
            }

        }

        return true;
    }

    bool MatchMaking::UpdateMatchQueue(int32_t eloValue)
    {
        if (NULL == m_pMatchElement)
        {
            return false;
        }
        if (m_pMatchElement->rank_strategy == E_MATCH_SUCCESS_CREATE_SCENE)
        {
            return UpdateMatchRankQueue(eloValue);
        }
        if (m_pMatchElement->rank_strategy == E_MATCH_SUCCESS_ENTER_TEAM)
        {
            return UpdateMatchNomalQueue(eloValue);
        }
        return false;
    }

    bool MatchMaking::UpdateMatchNomalQueue(int32_t eloValue)
    {
        group_queue_type::iterator it = m_GroupPlayer.find(eloValue);
        if (it == m_GroupPlayer.end())
        {
            return false;

        }
        match_queue_type & players = it->second;

        match_queue_type teamqueue;
        match_queue_type singlequeue;
        match_uid_list_type matched;

        for (auto && pi : players)
        {
            if (pi.second->IsTeamGroup())
            {
                teamqueue.emplace(pi.first, pi.second);
            }
            if (pi.second->IsSingle())
            {
                singlequeue.emplace(pi.first, pi.second);
            }
        }

        for (match_queue_type::iterator teamit = teamqueue.begin(); teamit != teamqueue.end(); ++teamit)
        {
            for (auto & singleit : singlequeue)
            {
				bool replace = false;
				Team::team_id_type teamId = teamit->second->GetTeamId();
				if (m_pTeamList->IsTeamFull(teamId)  && m_pTeamList->HasRobot(teamId))
				{
					replace = true;
				}
				if (replace)
				{
					teamit->second->SetSkipProfession(true);
				}
				if (OR_OK != teamit->second->CanMatch(*singleit.second))
				{
					teamit->second->SetSkipProfession(false);
					continue;
				}
				teamit->second->SetSkipProfession(false);

                if (teamit->second->GetPlayersSize(false) + singleit.second->GetPlayersSize() > Team::GetMaxSize(teamit->second->GetTeamType()))
                {
                    continue;
                }
                if (OR_OK != m_pTeamList->JoinTeam(teamId, singleit.second->GetSingleMemberInfo()))
                {
					bool cont = true;
					if (replace)
					{
						GUID_t role = m_pTeamList->ReplaceRobot(teamId, singleit.second->GetSingleMemberInfo());
						if (role)
						{
							if (m_pTeamList->GetInterface())
							{
								m_pTeamList->GetInterface()->LeaveTeam(teamId, role);
								if (m_pTeamList->JoinTeam(teamId, singleit.second->GetSingleMemberInfo()) == OR_OK)
								{
									m_pTeamList->GetInterface()->JoinTeam(teamId, singleit.second->GetSingleMemberInfo());
									cont = false;
								}
							}
						}
						if (m_pTeamList->IsTeamFull(teamId) && !m_pTeamList->HasRobot(teamId))
						{
							GUID_t role_id = singleit.second->GetSingleMemberInfo().roleid();
							CancelMatch(role_id, 1);
							cont = true;
						}
					}
					if(cont)
						continue;
                }
     
                matched.emplace(singleit.first);
                if (teamit->second->GetPlayersSize(false) < Team::GetMaxSize(teamit->second->GetTeamType()))
                {
                    continue;
                }
                for (auto & ji : teamit->second->GetPlayers())
                {
                    CancelMatch(ji.first);
                }
                matched.emplace(teamit->second->GetUnitId());
                break;
            }

            for (auto && ei : matched)
            {
                singlequeue.erase(ei);
                players.erase(ei);
            }
        }

        return true;
    }

    MatchMaking::match_queue_type MatchMaking::MatchTeamQueue(sort_match_map_type &  players,
        const match_queue_type & successList,
        std::size_t & matchsize,
        i32_v_type & vEraseIndex)
    {
        match_queue_type matched;
        matchsize = 0;
        i32_map_type professionList;
        for (auto && pi : players)
        {
            if (matchsize == GetPerTeamMemberMaxSize())
            {
                break;
            }
            if (matchsize + pi.second->GetPlayersSize() > GetPerTeamMemberMaxSize())
            {
                continue;
            }

            bool bCanMatch = true;

            for (auto && sIt : successList)
            {
                if (OR_OK != sIt.second->CanMatch(*pi.second))
                {
                    bCanMatch = false;
                    break;
                }
            }

            if (bCanMatch)
            {
                for (auto && mi : matched)
                {
                    if (OR_OK != mi.second->CanMatch(*pi.second))
                    {
                        bCanMatch = false;
                        break;
                    }
                }
            }

            if (!bCanMatch)
            {
                continue;
            }

            if (m_bWithoutRepetitionProfession)
            {
                bool bDifferentProfession = false;
                for (auto && ji : pi.second->GetProfession())
                {
                    if (professionList.find(ji.first) == professionList.end())
                    {
                        bDifferentProfession = true;
                        professionList.emplace(ji.first, 1);
                        break;
                    }
                }
                if (!bDifferentProfession && professionList.size() < GetMaxProfessionSize())
                {
                    continue;
                }
            }
            matched.emplace(pi.second->GetUnitId(), pi.second);
            vEraseIndex.push_back(pi.first);
            matchsize += pi.second->GetPlayersSize();
        }
        return matched;
    }

    MatchMaking::scene_type  MatchMaking::ComeOutAScene()
    {
        if (m_vScenes.empty())
        {
            return NULL;
        }
        MatchMaking::scene_type v = m_vScenes.begin()->second;
		RemoveScene(m_vScenes.begin()->first);
        return v;
    }

    void MatchMaking::ReMatch(match_queue_type & v, int32_t eloValue, group_id_type groupplayerId)
    {
        for (auto & it : v)
        {
            for (auto gi : it.second->GetPlayers())
            {
                m_vPlayers.erase(gi.first);
            }
        }
        for (auto & it : v)
        {
            Match(it.second);
        }
    }

    void MatchMaking::RemoveMatchUnit(match_unit_type & p)
    {
        for (auto & gi : p->GetPlayers())
        {
            m_vPlayers.erase(gi.first);
        }
    }

    void MatchMaking::RemoveScene(scene_id_type sceneId)
    {
        scenee_list_type::iterator it = m_vScenes.find(sceneId);
        if (it == m_vScenes.end())
        {
            return;
        }
		if (!it->second->HasRobot())
		{
			auto && vec = it->second->GetPlayers();

			for (auto & ji : vec)
			{
				auto pi = m_vPlayers.erase(ji.first);
			}
		}
        m_vScenes.erase(sceneId);
    }

    void MatchMaking::SceneTimeOut(scene_id_type sceneId)
    {
        scenee_list_type::iterator it = m_vScenes.find(sceneId);
        if (it == m_vScenes.end())
        {
            return;
        }
        it->second->TimeOut();
		RemoveScene(sceneId);
    }

    void MatchMaking::NotifySceneInfo(GUID_t guid)
    {
        scenee_list_type::iterator it = m_vScenes.find(GetSceneId(guid));
        if (it == m_vScenes.end())
        {
            return;
        }
        it->second->NotifySceneInfo(guid);
    }


    std::size_t MatchMaking::GetScenePlayerSize()
    {
        std::size_t rPlayerSize = 0;
        for (auto & it : m_vScenes)
        {
            rPlayerSize += it.second->GetScenePlayerSize();
        }
        return rPlayerSize;
    }

    std::size_t MatchMaking::GetScenePlayerSize(GUID_t nSceneId)
    {
        scenee_list_type::iterator it = m_vScenes.find(nSceneId);
        if (it == m_vScenes.end())
        {
            return 0;
        }
        return it->second->GetScenePlayerSize();
    }

    std::size_t MatchMaking::GetPlayerSize()
    {
        return m_vPlayers.size();
    }

    uint64_t MatchMaking::GetSceneId(GUID_t playerid)
    {
        palyers_type::iterator pi = m_vPlayers.find(playerid);
        if (pi == m_vPlayers.end())
        {
            if (m_pMatchElement->custom_scene <= 0)
            {
                return 0;
            }
            for (auto && ri : m_vScenes)
            {
                if (ri.second->HasPlayer(playerid))
                {
                    return ri.second->GetSceneId();
                }
            }
            return 0;
        }
        auto ji = m_vScenes.find(pi->second.m_nSceneId);
        if (ji == m_vScenes.end())
        {
            return 0;
        }

        return pi->second.m_nSceneId;
    }

    MatchMaking::scene_type MatchMaking::GetScene(GUID_t playerid)
    {
        palyers_type::iterator it = m_vPlayers.find(playerid);
        if (it == m_vPlayers.end())
        {
            return NULL;
        }

        scenee_list_type::iterator ri = m_vScenes.find(it->second.m_nSceneId);

        if (ri == m_vScenes.end())
        {
            return NULL;

        }
        return ri->second;
    }

    MatchMaking::scene_type MatchMaking::GetSceneFromSceneId(GUID_t nSceneId)
    {
        scenee_list_type::iterator ri = m_vScenes.find(nSceneId);

        if (ri == m_vScenes.end())
        {
            return NULL;

        }
        return ri->second;
    }

    int32_t MatchMaking::CheckCanMatchMemberSize(match_unit_type & mp)
    {
        if (NULL == m_pMatchElement)
        {
            return OR_MATCH_UNIT_TABLE_ERROR;
        }
        if (m_pMatchElement->rank_strategy == E_MATCH_SUCCESS_CREATE_SCENE)
        {
            if (IsRank())
            {
                if (mp->GetPlayersSize(false) > GetPerTeamMemberMaxSize() )
                {
                    return OR_MATCH_TEAM_CANT_MATCH__ERROR;
                }

                return OR_OK;
            }
        }

        if (mp->IsTeamGroup())
        {
            if (mp->GetPlayersSize(false) >= Team::GetMaxSize(mp->GetTeamType()))
            {
                return OR_MATCH_TEAM_CANT_MATCH__ERROR;
            }
        }
        else if (mp->GetPlayersSize(false) >= GetPerTeamMemberMaxSize())
        {
            return OR_MATCH_TEAM_CANT_MATCH__ERROR;
        }

        return OR_OK;
    }

    int32_t MatchMaking::SceneCancel(scene_id_type sceneId, GUID_t playerid)
    {
        scenee_list_type::iterator rit = m_vScenes.find(sceneId);
        if (rit == m_vScenes.end())
        {
            return OR_MATCH_SCENE_ERROR;
        }
        scene_type p = rit->second;
        OR_CHECK_RESULT(p->Cancel(playerid));
        RemoveScene(sceneId);
        return OR_OK;
    }

    int32_t MatchMaking::SceneReady(scene_id_type sceneId, GUID_t playerid)
    {
        scenee_list_type::iterator rit = m_vScenes.find(sceneId);
        if (rit == m_vScenes.end())
        {
            return OR_MATCH_SCENE_ERROR;
        }
        scene_type p = rit->second;
        OR_CHECK_RESULT(p->Ready(playerid));
        if (rit->second->IsAllReady())
        {
            RemoveScene(sceneId);
        }
        return OR_OK;
    }

    int32_t MatchMaking::OnEnterDungeon(scene_id_type sceneId)
    {
        scenee_list_type::iterator rit = m_vScenes.find(sceneId);
        if (rit == m_vScenes.end())
        {
            return OR_MATCH_SCENE_ERROR;
        }

        rit->second->OnEnterDungeon();
		RemoveScene(sceneId);
        return OR_OK;
    }

    std::size_t MatchMaking::GetNotInPlayerSceneSize()
    {
        std::size_t queueSize = 0;
        for (auto & it : m_vPlayers)
        {
            if (it.second.m_nSceneId == 0)
            {
                ++queueSize;
            }
        }
        return queueSize;
    }

    std::size_t MatchMaking::GetAllMatchUnitSize()
    {
        std::size_t n = 0;
        for (auto & it : m_GroupPlayer)
        {
            n += it.second.size();
        }
        return n;
    }

    void MatchMaking::receive(const TeamEventES& param)
    {
        int32_t eventId = param.team_event_type_;
        TeamList::team_id_type teamId = param.team_id_;
        GUID_t  m = param.guid_;
        int32_t nCancelAllTeam = 0;
        if (NULL != m_pMatchElement)
        {
            nCancelAllTeam = m_pMatchElement->member_leave_cancel_team;
        }
        static const int32_t kCancelAllTeamMatch = 1;
        switch (eventId)
        {
        case Team::E_BEFORE_LEADER_LEAVE_TEAM:
        {
            OR_CHECK_RESULT_RETURN_VOID(CancelMatch(m));
        }
        break;
        case Team::E_BEFORE_MEMBER_LEAVE_TEAM:
        {
            OR_CHECK_RESULT_RETURN_VOID(CancelMatch(m, nCancelAllTeam));
        }
        break;
        case Team::E_APPOINT_LEADER:
        {
            OR_CHECK_RESULT_RETURN_VOID(CancelMatch(m));
        }
        break;
        case Team::E_CHANGE_TEAM_TYPE:
        {
            OR_CHECK_RESULT_RETURN_VOID(CancelMatch(m, kCancelAllTeamMatch));
        }
        break;
        case  Team::E_CHANGE_TARGET_TYPE:
        {
            OR_CHECK_RESULT_RETURN_VOID(CancelMatch(m, kCancelAllTeamMatch));
        }
        break;
        case  Team::E_JOIN_TEAM:
        {
            GUID_t nLeaderRoleId = m_pTeamList->GetLearderRoleId(teamId);
           
            if (nLeaderRoleId > 0 && InMatch(nLeaderRoleId))
            {
                if (m_pTeamList->IsTeamFull(teamId))
                {
					if (!m_pTeamList->HasRobot(teamId))
					{
						OR_CHECK_RESULT_RETURN_VOID(CancelMatch(nLeaderRoleId, kCancelAllTeamMatch));
						OR_CHECK_RESULT_RETURN_VOID(CancelMatch(m, kCancelAllTeamMatch));
						return;
					}
                }

                match_unit_type pLeaderMatchUnit = GetMatchUnit(nLeaderRoleId);
                if (nullptr == pLeaderMatchUnit)
                {
                    OR_CHECK_RESULT_RETURN_VOID(CancelMatch(m, kCancelAllTeamMatch));
                    return;
                }
                match_unit_type pMyMatch = GetMatchUnit(m);
                if (nullptr ==  pMyMatch)
                {
                    TeamMemberInfo tMemberInfo = m_pTeamList->GetTeamMemberInfo(m);
                    MatchInfo mi;
                    TeamMemberToMatchInfo(tMemberInfo, mi);
                    mi.set_roleid(m);
                    pMyMatch.reset(new MatchUnit(pLeaderMatchUnit->MatchEloId(), pLeaderMatchUnit->GetMatchType(), mi));
                  
                }
              
                if (OR_OK != pLeaderMatchUnit->JoinPlayer(pMyMatch))
                {
                    OR_CHECK_RESULT_RETURN_VOID(CancelMatch(m, kCancelAllTeamMatch));
                    return;
                   
                }
                pMyMatch->SetUnitId(pLeaderMatchUnit->GetUnitId());

                OnPlayerMatch(pMyMatch);
            
            }
            else
            {
                OR_CHECK_RESULT_RETURN_VOID(CancelMatch(m, kCancelAllTeamMatch));
            }

        }
        break;
        default:
            break;
        }
    }

    int32_t MatchMaking::CreateCustomScene(match_unit_type & p)
    {
        OR_CHECK_RESULT(CheckRobot(p));
        CreateMatchRoomParam cp{ 
            p, 
            {}, 
            0,  
            m_nType , 
            kEmptyRematchCallback, 
            kEmptyRemoveUnitCallback, 
            m_pTeamList, 
            &p->GetMatchTarget(), 
            m_oSendCallBack,
            m_oSendGameCallBack,
			m_oMirrorMatchCallBack};
     
        return CreateCustomScene(cp);
    }

    int32_t MatchMaking::CreateCustomScene(CreateMatchRoomParam & cmp)
    {
        OR_CHECK_RESULT(CheckRobot(cmp.p_match_unit_));
       
        scene_type r(new MatchRoom(cmp));
        AddScene(r);
        return OR_OK;
    }

    int32_t MatchMaking::CreateCustomSceneFromTeamId(CreateMatchRoomParam & pp)
    {
        pp.p_team_list_ = m_pTeamList;
        MatchMaking::match_unit_type p_match_unint(new MatchUnit(pp.team_id_, m_pTeamList));
        pp.p_match_unit_ = p_match_unint;
        
        if (nullptr != pp.p_target_)
        {
            p_match_unint->SetMatchTarget(*pp.p_target_);
        }
        
        return CreateCustomScene(pp);
    }

    int32_t MatchMaking::CheckRobot(match_unit_type & p)
    {
        if (nullptr == p)
        {
            return OR_NULL_PTR;
        }
        if (NULL == m_pMatchElement)
        {
            return OR_MATCH_CUSTOM_SCENE_TABLE_ERROR_ERRO;
        }
        if (m_pMatchElement->custom_scene <= 0)
        {
            return OR_MATCH_CUSTOM_SCENE_TYPE_ERRO;
        }

        bool use_robot = false;
        GUID_t role_id = 0;
        for (auto && it : p->GetPlayers())
        {
            role_id = it.second.roleid();
            if (it.second.robot())
            {
                use_robot = true;
                break;
            }
        }
        if (!use_robot)
        {
            CancelMatch(role_id, 1);
        }
            
        if (!use_robot && InMatch(p))
        {
            return OR_MATCH_IN_MATCH_ERROR;
        }

        return OR_OK;

    }

    void MatchMaking::ChangeProfession(GUID_t roleId, int32_t p)
    {
        match_unit_type pu = GetMatchUnit(roleId);
        if (nullptr == pu)
        {
            return;
        }
        pu->SetProfession(roleId, p);
        UpdateMatchQueue(pu->MatchEloId());
    }

    void MatchMaking::UpdatePlayer(MatchInfo & mi)
    {
        match_unit_type pu = GetMatchUnit(mi.roleid());
        if (nullptr == pu)
        {
            return;
        }
        pu->UpdatePlayer(mi);
    }

    int32_t MatchMaking::SendMatchToClient(GUID_t roleId)
    {
        if (!m_oSendCallBack)
        {
            return OR_NULL_PTR;
        }
        auto p = GetMatchUnit(roleId);
        if (nullptr == p)
        {
            return OR_NULL_PTR;
        }
        return SendMatchToClient(roleId, p);
    }

    int32_t MatchMaking::SendMatchToClient(GUID_t roleId, match_unit_type& p)
    {
        if (!m_oSendCallBack)
        {
            return OR_NULL_PTR;
        }

        MatchRpcMatchBeginTimeNotifyNotify msg;
        msg.set_begintime(p->GetBeginTime());
        m_oSendCallBack(roleId, ModuleMatch::RPC_CODE_MATCH_MATCHBEGINTIMENOTIFY_NOTIFY, msg);

        MatchRpcEnterMatchNotify enter_msg;
        enter_msg.mutable_matchinfo()->set_elo(p->MatchEloId());
        enter_msg.mutable_matchinfo()->set_matchtype(p->GetMatchType());
        enter_msg.mutable_matchinfo()->set_auto_(1);
        m_oSendCallBack(roleId, ModuleMatch::RPC_CODE_MATCH_ENTERMATCH_NOTIFY, enter_msg);

        return OR_OK;
    }

    void MatchMaking::AddScene(scene_type & pr)
    {
        pr->SetEnterDungeonCallback(m_oEnterDungeonCallBack);
        pr->SetSendMsgCallback(m_oSendCallBack);
        m_vScenes.emplace(pr->GetSceneId(), pr);
        if (BaseModule::GetThreadLocalStorageLoop() == NULL)
        {
            return;
        }
        BaseModule::GetThreadLocalStorageLoop()->runAfter(kPrepareTime, std::bind(&MatchMaking::SceneTimeOut, this, pr->GetSceneId()));
    }

    MatchMaking::match_unit_type MatchMaking::GetMatchUnit(GUID_t  playerguid)
    {
        palyers_type::iterator rit = m_vPlayers.find(playerguid);
        if (rit == m_vPlayers.end())
        {
            return nullptr;
        }
        group_queue_type::iterator et = m_GroupPlayer.find(rit->second.m_nEloId);
        if (et == m_GroupPlayer.end())
        {
            return nullptr;
        }

        match_queue_type::iterator pi = et->second.find(rit->second.m_nMatchUnitId);
        if (pi == et->second.end())
        {
            return nullptr;
        }
        return pi->second;
    }

    void MatchMaking::SencCancelMatch(GUID_t playerguid)
    {
        if (m_oSendCallBack)
        {
            MatchRpcCancelMatchNotifyNotify msg;
            m_oSendCallBack(playerguid, ModuleMatch::RPC_CODE_MATCH_CANCELMATCHNOTIFY_NOTIFY, msg);
        }

        if (m_oSendGameCallBack)
        {
#ifndef __TEST__
            W2GEnterCancelMatch msg;
            msg.set_roleid(playerguid);
            m_oSendGameCallBack(playerguid, MatchType::W2G_MATCH_ENTER_CANCEL_MATCH, msg);
#endif // !__TEST__
        }

    }

}//namespace GameMMR