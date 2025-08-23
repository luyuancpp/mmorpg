#ifndef __MATCH_ROOM_H_
#define __MATCH_ROOM_H_

#include "TeamList.h"
#include "MatchUnit.h"
#include "PublicStruct.pb.h"

class CreateDungeonAsk;



namespace GameMMR
{

    static const int32_t kPrepareTime = 30;
    class CreateMatchRoomParam
    {
    public:
        typedef uint64_t match_unit_id;
        typedef uint64_t room_id_type;
        typedef std::shared_ptr<MatchUnit> match_unit_ptr_type;
        typedef std::unordered_map<GUID_t, MatchInfo> matchinfo_pb_list;
        typedef std::unordered_map<match_unit_id, match_unit_ptr_type> match_unit_list_type;
        typedef std::unordered_set<GUID_t> room_guids_type;
        typedef std::function<void(GUID_t, uint32_t, ::google::protobuf::Message&)> send_callback_type;
        typedef std::function<void(GUID_t, uint32_t, ::google::protobuf::Message&)> send_scene_callback_type;
        typedef std::vector<match_unit_list_type> camp_mu_list_type;
        typedef std::vector<camp_mu_list_type> room_camps_list_type;

        typedef std::function<void(match_unit_ptr_type&)> remove_unit_cb_type;
        typedef std::function<void(match_unit_list_type&, int32_t, uint64_t)> match_cb_type;
        typedef std::function<void(CreateDungeonAsk&)> enter_dungeon_callback;
		typedef std::function<void(int32_t, int32_t, int32_t, uint64_t)> mirror_match_callback_type;
        typedef std::function<void(Team::team_id_type)> team_all_ready_callback_type;

        static const match_cb_type kEmptyRematchCallback;
        static const remove_unit_cb_type  kEmptyRemoveUnitCallback;

        match_unit_ptr_type  p_match_unit_;
        room_camps_list_type  v;
        int32_t groupId{0};
        int32_t matchType{0};
        match_cb_type matchcb{ kEmptyRematchCallback };
        remove_unit_cb_type  rucb{ kEmptyRemoveUnitCallback };
        TeamList * p_team_list_{nullptr};
        const MatchTargetInfo * p_target_{ nullptr };
        send_callback_type sendcb;
        send_scene_callback_type send_gs_cb;
		mirror_match_callback_type m_oMirrorMatchCallBack;
        Team::team_id_type team_id_{Team::EmptyTeamId};
        team_all_ready_callback_type team_all_ready_callback_;
        int32_t prepare_seconds_{ kPrepareTime };
        std::string name;
        GUID_t create_guid_{ 0 };
    };

    class MatchRoom
    {
    public:

        enum eReadyStatus
        {
            E_STATUS_NONE,
            E_STATUS_READY,
        };
        typedef uint64_t match_unit_id;
        typedef uint64_t room_id_type;
     
        typedef std::shared_ptr<MatchUnit> match_unit_ptr_type;
        typedef std::unordered_map<GUID_t, MatchInfo> matchinfo_pb_list;
        typedef std::unordered_map<match_unit_id, match_unit_ptr_type> match_unit_list_type;
        typedef std::unordered_set<GUID_t> room_guids_type;
        typedef std::function<void(GUID_t, uint32_t, ::google::protobuf::Message&)> send_callback_type;
        typedef std::vector<match_unit_list_type> camp_mu_list_type;
        typedef std::vector<camp_mu_list_type> room_camps_list_type;

        typedef std::function<void(match_unit_ptr_type&)> remove_unit_cb_type;
        typedef std::function<void(match_unit_list_type&, int32_t, uint64_t)> match_cb_type;
        typedef std::function<void(CreateDungeonAsk&)> enter_dungeon_callback;
		typedef std::function<void(int32_t, int32_t, int32_t, uint64_t)> mirror_match_callback_type;
        typedef std::function<void(Team::team_id_type)> team_all_ready_callback_type;

        MatchRoom(CreateMatchRoomParam & cp);

        void ToClientPb(PBMatchRoom & pb);

        bool TestTeamInTheSameCamp(TeamInfo & teamInfo);
       
        uint64_t GetRoomId()
        {
            return m_nRoomId;
        }

        match_unit_list_type GetMatchUnit()
        {
            match_unit_list_type m;

            for (auto & it : m_vUnits)
            {
                 m.emplace(it.first, it.second);

            }
            return m;
        }

        matchinfo_pb_list GetPlayers()
        {
            return m_vMatchInfoList;
        }

        bool HasPlayer(GUID_t nRoleId)
        {
            return m_vMatchInfoList.find(nRoleId) != m_vMatchInfoList.end();
        }

        std::size_t GetRoomPlayerSize()
        {
            return m_vMatchInfoList.size();
        }

        i32_map_type GetPrefession()
        {
            i32_map_type v;
          
            for (auto & it : m_vMatchInfoList)
            {
                v.emplace(it.second.professionid(), 1);
            }
            return v;
        }

        int32_t Ready(GUID_t playerId);
		int32_t RobotReady(GUID_t playerId);
        int32_t Cancel(GUID_t playerId);

        void OnEnterDungeon();

        std::size_t GetReadySize();

        bool InRoome(GUID_t playerId);

        void TimeOut();

        void SetAllReady();
        bool IsAllReady();
		bool HasRobot();
        int32_t SetNone(GUID_t playerId);

        
        room_guids_type GetGuids();
        //for test
        room_guids_type GetTeamGuid(GUID_t p);
        GUID_t RandomGuid();
        void SetSendMsgCallback(send_callback_type o)
        {
            m_oSendCallBack = o;
        }
        void SetEnterDungeonCallback(enter_dungeon_callback o)
        {
            m_oEnterDungeonCallBack = o;
        }
		void SetMirrorMatchCallback(mirror_match_callback_type o)
		{
			m_oMirrorMatchCallBack = o;
		}

        void NotifyRoomInfo(GUID_t guid);

        std::string GetName() { return name_; }
        Team::team_id_type GetTeamId() { return team_id_; }
        GUID_t GetCreatePlayerId()const { return create_guid_; }
    private:
        void OnAllPlayerReady();
        void RemoveRoom();
        void InitTarget();
        void NotifyClientPrepare();
    private:
        int32_t m_nGroup{ 0 };
        int32_t m_nMatchType{ 0 };
        uint64_t m_nRoomId{0};
        match_unit_list_type m_vUnits;
        matchinfo_pb_list m_vMatchInfoList;
        static Snowflake m_oSnowflake;
        TeamList * m_p_team_list_;
        match_cb_type m_oReMatchCallBack;
        remove_unit_cb_type m_oRemoveMatchUnitCallBack;
        room_camps_list_type m_vCamp;
        send_callback_type m_oSendCallBack;
        MatchTargetInfo m_oTarget;
        enter_dungeon_callback m_oEnterDungeonCallBack;
		mirror_match_callback_type m_oMirrorMatchCallBack;
        team_all_ready_callback_type team_all_ready_callback_;
        Team::team_id_type team_id_{ Team::EmptyTeamId };
        time_t time_out_{0};
        std::string name_;
        GUID_t create_guid_{0};
    };



}//namespace GameMMR




#endif // !__MATCH_ROOM_H_
