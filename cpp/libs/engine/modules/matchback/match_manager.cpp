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


    MatchMaking::room_type  MatchManager::ComeOutRestult(int32_t matchType)
    {
        if (OR_OK != CheckMatchType(matchType))
        {
            return NULL;
        }
        return m_vMatchList[matchType]->ComeOutARoom();
  
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

    std::size_t MatchManager::GetRoomSize(int32_t matchType)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->GetRoomSize();
    }

    std::size_t MatchManager::GetNotInPlayerRoomSize(int32_t matchType)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->GetNotInPlayerRoomSize();
    }

    std::size_t MatchManager::GetRoomSize()
    {
        std::size_t allsize = 0;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            allsize += (*it)->GetRoomSize();
        }
        return allsize;
    }

    std::size_t MatchManager::GetRoomPlayerSize(int32_t matchType, GUID_t nRoomId)
    {
        if (matchType < 0 || matchType >= (int32_t)m_vMatchList.size())
        {
            return 0;
        }

        return m_vMatchList[matchType]->GetRoomPlayerSize(nRoomId);
    }

    uint64_t MatchManager::GetRoomId( GUID_t playerid)
    {
        uint64_t roomId = 0;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
       
            roomId = (*it)->GetRoomId(playerid);
            if (roomId > 0)
            {
                return roomId;
            }
          
        }

        return roomId;
    }


    MatchMaking::room_type MatchManager::GetRoom(GUID_t playerid)
    {
        MatchMaking::room_type roomPtr;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {
            roomPtr = (*it)->GetRoom(playerid);
            if (NULL != roomPtr)
            {
                return roomPtr;
            }
        }

        return roomPtr;
    }

    ui64_set_type MatchManager::GetRoomPlayeridsByPlayerId(GUID_t playerid)
    {
        ui64_set_type v;
        auto p_room = GetRoom(playerid);
        if (nullptr == p_room)
        {
            return v;
        }

        return p_room->GetGuids();
    }

    MatchMaking::room_type MatchManager::GetRoomFromRoomId(uint64_t roomId)
    {
        MatchMaking::room_type roomPtr;
        for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
        {

            roomPtr = (*it)->GetRoomFromRoomId(roomId);
            if (NULL != roomPtr)
            {
                return roomPtr;
            }

        }

        return roomPtr;
    }

    int32_t MatchManager::RoomCancel(int32_t matchType, uint64_t roomeId, GUID_t playerid)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->RoomCancel(roomeId, playerid);
    }

    int32_t MatchManager::RoomReady(int32_t matchType, uint64_t roomId, GUID_t playerid)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->RoomReady(roomId, playerid);
    }

    int32_t MatchManager::CustomRoomCancel(GUID_t playerid)
    {
        GUID_t room_id = GetRoomId(playerid);
        return RoomCancel(GameMMR::E_NOMAL_TEAM_MATCH, room_id, playerid);
    }

    int32_t MatchManager::CustomRoomReady(GUID_t playerid)
    {
        GUID_t room_id = GetRoomId(playerid);
        return RoomReady(GameMMR::E_NOMAL_TEAM_MATCH, room_id, playerid);
    }

    void MatchManager::NotifyRoomInfo(GUID_t guid)
    {
        for (auto&it : m_vMatchList)
        {
            it->NotifyRoomInfo(guid);
        }
    }

    int32_t MatchManager::OnEnterDungeon(int32_t matchType, uint64_t roomId)
    {
        OR_CHECK_RESULT(CheckMatchType(matchType));

        return m_vMatchList[matchType]->OnEnterDungeon(roomId);
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

    int32_t MatchManager::CreateCustomRoom(int32_t nType, player_ptr_type & p)
    {
        OR_CHECK_RESULT(CheckMatchType(nType));
        OR_CHECK_RESULT(m_vMatchList[nType]->CreateCustomRoom(p));
        return OR_OK;
    }

    int32_t MatchManager::CreateCustomRoom(CreateMatchRoomParam & p)
    {
        OR_CHECK_RESULT(CheckMatchType(p.matchType));
        OR_CHECK_RESULT(m_vMatchList[p.matchType]->CreateCustomRoom(p));
        return OR_OK;
    }

    int32_t MatchManager::CreateCustomRoomFromTeamId(CreateMatchRoomParam & p)
    {
        OR_CHECK_RESULT(CheckMatchType(E_NOMAL_TEAM_MATCH));
        OR_CHECK_RESULT(m_vMatchList[E_NOMAL_TEAM_MATCH]->CreateCustomRoomFromTeamId(p));
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