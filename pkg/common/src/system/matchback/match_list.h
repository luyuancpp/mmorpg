#ifndef __MatchMaking_h
#define __MatchMaking_h


#include <unordered_set>



#include "MatchRoom.h"
#include "TeamList.h"

#include "GameTimer.h"
#include "Game/StlDefineType.h"
#include "CommonLogic/GameEvent/GameEvent.h"

class CreateDungeonAsk;

namespace GameMMR
{


    class MatchMaking : public entityx::Receiver<MatchMaking>
    {
    public:

        struct PrivePlayerInfo
        {
            uint64_t m_nMatchUnitId{0};
            uint64_t m_nRoomId{ 0 };
            int32_t m_nEloId{ 0 };
        };

        enum eMatchStartegyEnum
        {
            E_MATCH_SUCCESS_ENTER_TEAM,
            E_MATCH_SUCCESS_CREATE_ROOM,
        };

        typedef uint64_t group_id_type;
        typedef uint64_t room_id_type;

        typedef std::shared_ptr<MatchRoom> room_type;
        typedef std::shared_ptr<MatchUnit> match_unit_type;
        typedef std::unordered_map<uint64_t, match_unit_type> match_queue_type;
        typedef std::vector<match_unit_type> sort_match_vec_type;
        typedef std::map<uint32_t, match_unit_type> sort_match_map_type;
        typedef std::unordered_map<int32_t, match_queue_type> group_queue_type;
        typedef std::unordered_set<uint64_t> match_uid_list_type;

        typedef std::unordered_map<room_id_type, room_type> roome_list_type;
        typedef std::unordered_map<GUID_t, PrivePlayerInfo> palyers_type;

        typedef std::vector<match_queue_type> camp_teams_type;
        typedef std::vector<camp_teams_type> room_camps_vec_type;
       
        typedef std::function<void(int32_t, GUID_t)> EventCallback;
        typedef std::function<void(GUID_t, uint32_t, ::google::protobuf::Message&)> send_callback_type;
        typedef std::function<void(GUID_t, uint32_t, ::google::protobuf::Message&)> send_scene_callback_type;
        typedef std::function<void(CreateDungeonAsk&)> enter_dungeon_callback;
		typedef std::function<void(int32_t, int32_t, int32_t, uint64_t)> mirror_match_callback_type;

        MatchMaking(int32_t matchType, TeamList * oTeamList);

        int32_t Match(match_unit_type & p);

        bool InMatch(match_unit_type & p);
        bool InMatch(GUID_t  playerguid);

        bool InGroupMatch(GUID_t  playerguid);

        //只有手动和退出房间的时候才能从匹配队列删除
        int32_t CancelMatch(GUID_t  playerguid, int32_t nCancelSelfMatchUnitTeam = 1);
        


        std::size_t GetPlayerSize();

        bool UpdateMatchRankQueue(int32_t groupValue);
        bool UpdateMatchRankQueue(int32_t groupValue, sort_match_map_type & players);
        
        bool UpdateMatchQueue(int32_t groupValue);
        bool UpdateMatchNomalQueue(int32_t groupValue);
        match_queue_type MatchTeamQueue(sort_match_map_type &  players,
                                        const match_queue_type & successList,
                                        std::size_t & nOutSize,
                                        i32_v_type & vEraseIndex);

        MatchMaking::room_type  ComeOutARoom();

        inline std::size_t GetPerCampPlayerSize()const
        {
            return m_nkPerCampPlayerSize;
        }

        inline std::size_t GetMaxRoomCampSize()const
        {
            return m_nkMaxRoomCampSize;
        }

        inline std::size_t GetMaxProfessionSize()const
        {
            return m_nkMaxProfessionSize;
        }

        inline  std::size_t GetRoomMaxPlayerSize()
        {
            return GetPerCampPlayerSize() * GetMaxRoomCampSize();
        }

        inline std::size_t GetPerTeamMemberMaxSize()
        {
            return m_nkPerTeamMemberMaxSize;
        }

        void ReMatch(match_queue_type & v, int32_t groupValue, group_id_type groupplayerId);
        void RemoveMatchUnit(match_unit_type & p);
        void RemoveRoom(room_id_type roomId);
        void RoomTimeOut(room_id_type roomId);
        void NotifyRoomInfo(GUID_t guid);
        
        std::size_t GetRoomPlayerSize();
        std::size_t GetRoomPlayerSize(GUID_t nRoomId);

        uint64_t GetRoomId(GUID_t playerid);
        room_type GetRoom(GUID_t playerid);
        room_type GetRoomFromRoomId(GUID_t nRoomId);

        int32_t CheckCanMatchMemberSize(match_unit_type & mp);

        int32_t RoomCancel(room_id_type roomId, GUID_t playerid);
        int32_t RoomReady(room_id_type roomId, GUID_t playerid);
        int32_t OnEnterDungeon(room_id_type roomId);

        void SetWithoutRepetitionProfession(bool bCheckProfession)
        {
            m_bWithoutRepetitionProfession = bCheckProfession;
        }

        std::size_t GetRoomSize()const
        {
            return m_vRooms.size();
        }

        std::size_t GetNotInPlayerRoomSize();

        std::size_t GetAllMatchUnitSize();
#ifdef __TEST__

        roome_list_type & GetRooms()
        {
            return m_vRooms;
        }

        sort_match_map_type  GetMatchListForTest(int32_t nGroupId)
        {
            sort_match_map_type v;
            group_queue_type::iterator groupIt = m_GroupPlayer.find(nGroupId);
            if (groupIt == m_GroupPlayer.end())
            {
                return v;
            }
         
            GetGroupSortQueue(nGroupId, v);
            return v;
        }
#endif // __TEST__

        void receive(const TeamEventES& param);

        void SetSendMsgCallback(send_callback_type o)
        {
            m_oSendCallBack = o;
        }

        void SetSendGameMsgCallback(const send_scene_callback_type & o)
        {
            m_oSendGameCallBack = o;
        }
        
        void SetEnterDungeonCallback(enter_dungeon_callback o);

		void SetMirrorMatchCallback(mirror_match_callback_type o)
		{
			m_oMirrorMatchCallBack = o;
		}

        bool IsRank()const;

        int32_t CreateCustomRoom(match_unit_type & p);
        int32_t CreateCustomRoom(CreateMatchRoomParam & cmp);
        int32_t CreateCustomRoomFromTeamId(CreateMatchRoomParam & p);
        int32_t CheckRobot(match_unit_type & p);

        void ChangeProfession(GUID_t roleId, int32_t p);
        void UpdatePlayer(MatchInfo & mi);

        int32_t SendMatchToClient(GUID_t roleId);
        int32_t SendMatchToClient(GUID_t roleId, match_unit_type& p);
private:
        int32_t OnPlayerMatch(match_unit_type & p);
        bool InGroupTestMatch(GUID_t  playerguid);
        bool InRoom(GUID_t  playerguid);
        int32_t MatchSuccess(GUID_t  playerguid);
        void GetGroupSortQueue(int32_t  groupValue, sort_match_map_type & mv);
        void AddRoom(room_type & pr);

        match_unit_type GetMatchUnit(GUID_t  playerguid);

        void SencCancelMatch(GUID_t  playerguid);
private:
   
        group_queue_type m_GroupPlayer;
        palyers_type m_vPlayers;
        roome_list_type m_vRooms;
        bool m_bWithoutRepetitionProfession{false};
        int32_t m_nType{0};
        TeamList * m_pTeamList;
        EventCallback m_oEventCallback;
        send_callback_type m_oSendCallBack;
        send_scene_callback_type m_oSendGameCallBack;
        enter_dungeon_callback m_oEnterDungeonCallBack;
		mirror_match_callback_type m_oMirrorMatchCallBack;
        const MatchElement * m_pMatchElement{ NULL };
        std::size_t m_nkPerTeamMemberMaxSize{ 5 };
        std::size_t m_nkPerCampPlayerSize{ 1 };
        std::size_t m_nkMaxRoomCampSize{ 1 };
        std::size_t m_nkMaxProfessionSize{ 3 };
   
    };


}//namespace GameMMR



#endif // __MatchMaking_h

