#pragma once


#include "TeamList.h"

#include "MatchUnit.h"

#include "MatchMaking.h"

class CreateDungeonAsk;

namespace GameMMR
{

    class MatchManager
    {
    public:

        typedef std::shared_ptr<MatchMaking> match_making_ptr_type;
        typedef std::vector<match_making_ptr_type> match_list_type;
        typedef std::shared_ptr<MatchUnit> player_ptr_type;

        typedef std::function<void(int32_t, GUID_t)> EventCallback;
        typedef std::function<void(GUID_t, uint32_t, ::google::protobuf::Message&)> send_callback_type;
        typedef std::function<void(GUID_t, uint32_t, ::google::protobuf::Message& )> send_scene_callback_type;
        typedef std::function<void(CreateDungeonAsk&)> enter_dungeon_callback;
		typedef std::function<void(int32_t, int32_t, int32_t, uint64_t)> mirror_match_callback_type;

        MatchManager(TeamList * oTeamList);
        MatchManager();

        TeamList & GetTeamList()
        {
            return *m_pTeamList;
        }


        void Init(TeamList * p_team, int32_t match_module_id);

        int32_t Match(int32_t nType, player_ptr_type & p);

        bool InMatch(GUID_t  playerguid);
        bool InGroupMatch(GUID_t  playerguid);
        int32_t CancelMatch(GUID_t  playerguid);
        int32_t SendMatchToClient(GUID_t  playerguid);

        MatchMaking::scene_type  ComeOutRestult(int32_t matchType);

        int32_t CheckMatchType(int32_t matchType);


        std::size_t GetPlayerSize(int32_t matchType);
        std::size_t GetPlayerSize();


        std::size_t GetSceneSize(int32_t matchType);
        std::size_t GetNotInPlayerSceneSize(int32_t matchType);
        std::size_t GetSceneSize();
        std::size_t GetScenePlayerSize(int32_t matchType, GUID_t nSceneId);

        uint64_t GetSceneId( GUID_t playerid);
        MatchMaking::scene_type GetScene(GUID_t playerid);
        ui64_set_type GetScenePlayeridsByPlayerId(GUID_t playerid);
        MatchMaking::scene_type GetSceneFromSceneId(uint64_t sceneId);

        int32_t SceneCancel(int32_t matchType, uint64_t sceneId, GUID_t playerid);
        int32_t SceneReady(int32_t matchType, uint64_t sceneId, GUID_t playerid);
        int32_t CustomSceneCancel(GUID_t playerid);
        int32_t CustomSceneReady(GUID_t playerid);
        void NotifySceneInfo(GUID_t guid);

        int32_t OnEnterDungeon(int32_t matchType, uint64_t sceneId);

        void SetWithoutRepetitionProfession(bool bCheckProfession)
        {
            for (auto & it : m_vMatchList)
            {
                it->SetWithoutRepetitionProfession(bCheckProfession);
            }
        }


        std::size_t GetAllMatchUnitSize(int32_t matchType);
#ifdef __TEST__

        MatchMaking::scenee_list_type & GetScenes(int32_t matchType)
        {
            
            return m_vMatchList[matchType]->GetScenes();
        }

        MatchMaking::sort_match_map_type GetMatchListForTest(int32_t nMatchType, int32_t nGroup)
        {
            MatchMaking::sort_match_map_type v;
            if (nMatchType < 0 || nMatchType >= E_MATCH_MAX)
            {
                return v;
            }

            return m_vMatchList[nMatchType]->GetMatchListForTest(nGroup);
        }
#endif // __TEST__

        int32_t Match(int32_t nType, int32_t nElo, const TeamInfo & info);
        int32_t Match(int32_t nType, int32_t nElo, const TeamMemberInfo & info);


        void SetSendMsgCallback(send_callback_type o)
        {
            m_oSendCallBack = o;
            for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
            {
                (*it)->SetSendMsgCallback(m_oSendCallBack);
            }
        }

        void SetSendGameMsgCallback(const send_scene_callback_type& o)
        {
            m_oSendGameCallBack = o;
            for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
            {
                (*it)->SetSendGameMsgCallback(m_oSendGameCallBack);
            }
        }
      
        void SetEnterDungeonCallback(enter_dungeon_callback o)
        {
            m_oEnterDungeonCallBack = o;
            for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
            {
                (*it)->SetEnterDungeonCallback(m_oEnterDungeonCallBack);
            }
        }

		void SetMirrorMatchCallback(const mirror_match_callback_type& o)
		{
			m_oMirrorMatchCallBack = o;
			for (match_list_type::iterator it = m_vMatchList.begin(); it != m_vMatchList.end(); ++it)
			{
				(*it)->SetMirrorMatchCallback(m_oMirrorMatchCallBack);
			}
		}
        

        int32_t CreateCustomScene(int32_t nType, player_ptr_type & p);
        int32_t CreateCustomScene(CreateMatchRoomParam & p);
        int32_t CreateCustomSceneFromTeamId(CreateMatchRoomParam & p);

        void ChangeProfession(GUID_t roleId, int32_t p);
        void UpdatePlayer(MatchInfo & mi);

        
    private:

        match_list_type m_vMatchList;
        TeamList * m_pTeamList{nullptr};
        EventCallback m_oEventCallback;
        send_callback_type m_oSendCallBack;
        send_scene_callback_type m_oSendGameCallBack;
        enter_dungeon_callback m_oEnterDungeonCallBack;
		mirror_match_callback_type m_oMirrorMatchCallBack;	//mirror match ready callback
    };
}//namespace GameMMR
