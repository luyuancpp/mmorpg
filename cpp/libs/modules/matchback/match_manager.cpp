#include "MatchMakingManager.h"

#include "GenCode/GameDefine_Result.h"

namespace GameMMR
{
    MatchManager::EventCallback kEmptyManagerEventCallback;
 

    MatchManager::MatchManager(TeamList * oTeamList)
        : m_pTeamList(oTeamList)
    {
#ifdef __TEST__
        Init(m_pTeamList, E_WORLD_MATCH_MAKING_SERVER_MODULE);
#endif // __TEST__

    }

    MatchManager::MatchManager()
    {
    }

    void MatchManager::Init(TeamList * p_team, int32_t match_module_id)
    {
        m_pTeamList = p_team;
        m_vMatchList.resize(E_MATCH_MAX);
        m_vMatchList[E_NOMAL_TEAM_MATCH].reset(new MatchMaking(E_NOMAL_TEAM_MATCH, m_pTeamList));
        m_vMatchList[E_RELATION_UNION_ALLY].reset(new MatchMaking(E_RELATION_UNION_ALLY, m_pTeamList));
        m_vMatchList[E_CROSS_SERVER_MATCH_DUNGEON_TYPE].reset(new MatchMaking(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, m_pTeamList));
        m_vMatchList[E_MATCH_3V3].reset(new MatchMaking(E_MATCH_3V3, m_pTeamList));
        m_vMatchList[E_MATCH_15V15].reset(new MatchMaking(E_MATCH_15V15, m_pTeamList));
        m_vMatchList[E_TEST_NOMAL_TEAM_RANK_MATCH].reset(new MatchMaking(E_TEST_NOMAL_TEAM_RANK_MATCH, m_pTeamList));
        m_vMatchList[E_TEST_GROUP_MATCH].reset(new MatchMaking(E_TEST_GROUP_MATCH, m_pTeamList));
        
       
    }

    int32_t MatchManager::Match(int32_t nType, player_ptr_type & p)
    {
        OR_CHECK_RESULT(CheckMatchType(nType));
        OR_CHECK_RESULT(m_vMatchList[nType]->Match(p));
        return OR_OK;
    }

    bool MatchManager::InMatch(GUID_t  playerguid)
    {
        for (match_list_type::iterator  it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            if ((*it)->InMatch(playerguid))
            {
                return true;
            }
        }
        return false;
    }

    bool MatchManager::InGroupMatch(GUID_t  playerguid)
    {
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            if ((*it)->InGroupMatch(playerguid))
            {
                return true;
            }
        }
        return false;
    }

    int32_t MatchManager::CancelMatch(GUID_t  playerguid)
    {
        int32_t nRet = OR_OK;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            nRet = (*it)->CancelMatch(playerguid);
            if (OR_OK == nRet)
            {
                return OR_OK;
            }
        }
        return nRet;
    }

    int32_t MatchManager::SendMatchToClient(GUID_t  playerguid)
    {
        int32_t nRet = OR_OK;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            nRet = (*it)->SendMatchToClient(playerguid);
            if (OR_OK == nRet)
            {
                return OR_OK;
            }
        }
        return nRet;
    }


    MatchMaking::scene_type  MatchManager::ComeOutRestult(int32_t matchType)
    {
        if (OR_OK != CheckMatchType(matchType))
        {
            return nullptr;
        }
        return m_vMatchList[matchType]->ComeOutAScene();
  
    }

    int32_t MatchManager::CheckMatchType(int32_t matchType)
    {
        if (matchType < 0 || matchType >= (int32_t)m_vMatchList.size())
        {
            return OR_MATCH_TYPE_ERROR;
        }

        return OR_OK;
    }


    std::size_t MatchManager::GetPlayerSize(int32_t matchType)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->GetPlayerSize();
    }

    std::size_t MatchManager::GetPlayerSize()
    {
        std::size_t allsize = 0;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            allsize += (*it)->GetPlayerSize();
        }
        return allsize;
    }

    std::size_t MatchManager::GetSceneSize(int32_t matchType)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->GetSceneSize();
    }

    std::size_t MatchManager::GetNotInPlayerSceneSize(int32_t matchType)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->GetNotInPlayerSceneSize();
    }

    std::size_t MatchManager::GetSceneSize()
    {
        std::size_t allsize = 0;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            allsize += (*it)->GetSceneSize();
        }
        return allsize;
    }

    std::size_t MatchManager::GetScenePlayerSize(int32_t matchType, GUID_t nSceneId)
    {
        if (matchType < 0 || matchType >= (int32_t)m_vMatchList.size())
        {
            return 0;
        }

        return m_vMatchList[matchType]->GetScenePlayerSize(nSceneId);
    }

    uint64_t MatchManager::GetSceneId( GUID_t playerid)
    {
        uint64_t sceneId = 0;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
       
            sceneId = (*it)->GetSceneId(playerid);
            if (sceneId > 0)
            {
                return sceneId;
            }
          
        }

        return sceneId;
    }


    MatchMaking::scene_type MatchManager::GetScene(GUID_t playerid)
    {
        MatchMaking::scene_type scenePtr;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            scenePtr = (*it)->GetScene(playerid);
            if (nullptr != scenePtr)
            {
                return scenePtr;
            }
        }

        return scenePtr;
    }

    ui64_set_type MatchManager::GetScenePlayeridsByPlayerId(GUID_t playerid)
    {
        ui64_set_type v;
        auto p_scene = GetScene(playerid);
        if (nullptr == p_scene)
        {
            return v;
        }

        return p_scene->GetGuids();
    }

    MatchMaking::scene_type MatchManager::GetSceneFromSceneId(uint64_t sceneId)
    {
        MatchMaking::scene_type scenePtr;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {

            scenePtr = (*it)->GetSceneFromSceneId(sceneId);
            if (nullptr != scenePtr)
            {
                return scenePtr;
            }

        }

        return scenePtr;
    }

    int32_t MatchManager::SceneCancel(int32_t matchType, uint64_t sceneeId, GUID_t playerid)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->SceneCancel(sceneeId, playerid);
    }

    int32_t MatchManager::SceneReady(int32_t matchType, uint64_t sceneId, GUID_t playerid)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->SceneReady(sceneId, playerid);
    }

    int32_t MatchManager::CustomSceneCancel(GUID_t playerid)
    {
        GUID_t scene_id = GetSceneId(playerid);
        return SceneCancel(GameMMR::E_NOMAL_TEAM_MATCH, scene_id, playerid);
    }

    int32_t MatchManager::CustomSceneReady(GUID_t playerid)
    {
        GUID_t scene_id = GetSceneId(playerid);
        return SceneReady(GameMMR::E_NOMAL_TEAM_MATCH, scene_id, playerid);
    }

    void MatchManager::NotifySceneInfo(GUID_t guid)
    {
        for (auto&it : m_vMatchList)
        {
            it->NotifySceneInfo(guid);
        }
    }

    int32_t MatchManager::OnEnterDungeon(int32_t matchType, uint64_t sceneId)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->OnEnterDungeon(sceneId);
    }

    std::size_t MatchManager::GetAllMatchUnitSize(int32_t matchType)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->GetAllMatchUnitSize();
    }

    int32_t MatchManager::Match(int32_t nType, int32_t nElo, const TeamInfo & info)
    {
        player_ptr_type p(new MatchUnit(nElo, nType, info));
        return Match(nType, p);
    }

    int32_t MatchManager::Match(int32_t nType, int32_t nElo, const TeamMemberInfo & info)
    {
        MatchInfo mi;

        mi.set_roleid(info.roleid());
        mi.set_level(info.level());
        mi.set_name(info.name());
        mi.set_configid(info.configid());
        mi.set_professionid(info.professionid());
		mi.set_robot(info.robot());
		mi.set_ghostguid(info.ghostguid());
        TeamMemberToMatchInfo(info, mi);
        player_ptr_type p(new MatchUnit(nElo, nType, mi));

        return Match(nType, p);
    }

    int32_t MatchManager::CreateCustomScene(int32_t nType, player_ptr_type & p)
    {
        OR_CHECK_RESULT(CheckMatchType(nType));
        OR_CHECK_RESULT(m_vMatchList[nType]->CreateCustomScene(p));
        return OR_OK;
    }

    int32_t MatchManager::CreateCustomScene(CreateMatchRoomParam & p)
    {
        OR_CHECK_RESULT(CheckMatchType(p.matchType));
        OR_CHECK_RESULT(m_vMatchList[p.matchType]->CreateCustomScene(p));
        return OR_OK;
    }

    int32_t MatchManager::CreateCustomSceneFromTeamId(CreateMatchRoomParam & p)
    {
        OR_CHECK_RESULT(CheckMatchType(E_NOMAL_TEAM_MATCH));
        OR_CHECK_RESULT(m_vMatchList[E_NOMAL_TEAM_MATCH]->CreateCustomSceneFromTeamId(p));
        return OR_OK;
    }

    void MatchManager::ChangeProfession(GUID_t roleId, int32_t p)
    {
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            (*it)->ChangeProfession(roleId, p);
        }
    }

    void MatchManager::UpdatePlayer(MatchInfo & mi)
    {
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            (*it)->UpdatePlayer(mi);
        }
    }
}//namespace GameMMR