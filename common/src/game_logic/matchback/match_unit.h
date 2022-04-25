#ifndef __MATCH_PALYER_H_
#define __MATCH_PALYER_H_


#include <unordered_map>

#include "BaseDef.h"
#include "Snowflake.h"

#include "PublicStruct.pb.h"

#include "Team.h"
#include "Game/StlDefineType.h"

class TeamList;
struct MatchElement;
struct TeamMatchingElement;

namespace GameMMR
{

    void TeamMemberToMatchInfo(const TeamMemberInfo& ti, MatchInfo& mi);
    void MatchInfoToTeamMember(const MatchInfo& mi, TeamMemberInfo& ti);

    enum eMatchType
    {
        E_NOMAL_TEAM_MATCH = 0,
        E_RELATION_UNION_ALLY = 1,//
        E_CROSS_SERVER_MATCH_DUNGEON_TYPE,//¿ç·þÆ¥Åä
        E_MATCH_3V3,
        E_MATCH_15V15,
        E_TEST_NOMAL_TEAM_RANK_MATCH,
        E_TEST_GROUP_MATCH,
        E_MATCH_MAX
    };

    enum eMatchFilterType
    {
        E_MATCH_FILTER_NONE,
        E_MATCH_FILTER_GUILD,
        E_MATCH_FILTER_MAX,
    };

    class MatchUnit
    {
    public:
        typedef std::unordered_map<GUID_t, MatchInfo> pb_player_list_type;
        typedef std::shared_ptr<MatchUnit> match_unit_ptr_type;
        typedef std::unordered_map<GUID_t, MatchRankInfo> player_integral_map_type;

        static const player_integral_map_type kEmptyMatchRankInfo;
        static const MatchTargetInfo kEmptyMatchTarget;

        MatchUnit(int32_t nElo,
            int32_t nMatchType,
            const TeamInfo & info,
            const player_integral_map_type & kRankInfo = kEmptyMatchRankInfo,
            const MatchTargetInfo & oMatchTarget = kEmptyMatchTarget
        );

        MatchUnit(int32_t nElo, 
            int32_t nMatchType, 
            const  MatchInfo & info,
            const player_integral_map_type & kRankInfo = kEmptyMatchRankInfo,
            const MatchTargetInfo & oMatchTarget = kEmptyMatchTarget
        );

        //custrom room
        MatchUnit(Team::team_id_type team_id, TeamList * p_team_list);

        static void TeamToMatchInfo(const TeamInfo & info, pb_player_list_type & players)
        {
            for (int32_t i = 0; i < info.members_size(); ++i)
            {
                MatchInfo mi;
                TeamMemberToMatchInfo(info.members(i), mi);
                players.emplace(mi.roleid(), mi);
            }
        }

        void OnInit();

        int64_t GetBeginTime() const { return begin_time_; }

        void SetRankInfo(const player_integral_map_type & kIntegrals = kEmptyMatchRankInfo)
        {
            for (auto && it : kIntegrals)
            {
                pb_player_list_type::iterator p = m_vMyPlayers.find(it.first);
                if (p == m_vMyPlayers.end())
                {
                    continue;
                }
                p->second.set_integral(it.second.integral());
                p->second.set_section(it.second.section());
            }
            for(pb_player_list_type::iterator pi = m_vMyPlayers.begin(); pi != m_vMyPlayers.end(); ++pi)
            {
                if (pi->second.section() == 0)
                {
                    pi->second.set_section(1);
                }
            }
        }

        bool HasPlayer(GUID_t palyerId);

        pb_player_list_type GetPlayers()
        {
            return m_vMyPlayers;
        }
		bool HasRobot()
		{
			for (pb_player_list_type::iterator pi = m_vMyPlayers.begin(); pi != m_vMyPlayers.end(); ++pi)
			{
				if (pi->second.robot())
				{
					return true;
				}
			}
			return false;
		}
		int RobotCount()const
		{
			int count = 0;
			for (pb_player_list_type::const_iterator pi = m_vMyPlayers.begin(); pi != m_vMyPlayers.end(); ++pi)
			{
				if (pi->second.robot())
				{
					count++;
				}
			}
			return count;
		}
        std::size_t GetPlayersSize(bool withrobot = true)const
        {
			return withrobot ? m_vMyPlayers.size() : (m_vMyPlayers.size() - RobotCount());
        }

        int32_t MatchEloId()
        {
            return elo_id_;
        }

        uint64_t GetUnitId()
        {
            return uint_id_;
        }

        void SetUnitId(uint64_t nUnitId)
        {
            uint_id_ = nUnitId;
        }

        bool IsSingle()const
        {
            return m_nTeamId == Team::EmptyTeamId;
        }

        bool IsTeamGroup()const
        {
            return !IsSingle();
        }

        i32_map_type & GetProfession()
        {
            return m_vUnitPrefessions;
        }

        int32_t JoinPlayer(match_unit_ptr_type & rhf);


        Team::team_id_type GetTeamId()
        {
            return m_nTeamId;
        }

        TeamMemberInfo & GetSingleMemberInfo()
        {
            return m_oTeamMemberInfo;
        }

        void PlayerCancel(GUID_t palyerId);

        bool IsEmpty();

        int32_t CanMatch( MatchUnit & r);
        int32_t CanMatch();

        const MatchLimit & GetMatchLimit()const
        {
            return m_oMatchLimit;
        }

      
        bool operator < (const  MatchUnit &)const;

        void TestOutPut();

        int32_t GetAvergeIntegral()const;

        const MatchTargetInfo & GetMatchTarget()
        {
            return m_oMatchTarget;
        }

        int32_t GetMatchType()
        {
            return m_nMatchType;
        }

        int32_t GetTeamType()const
        {
            return m_nTeamType;
        }

        void SetProfession(GUID_t roleId , int32_t p);

        void UpdatePlayer(MatchInfo & mi);

		void SetSkipProfession(bool val) { m_SkipProfession = val; }
        void SetMatchTarget(const MatchTargetInfo & m) { m_oMatchTarget = m; }
    private:
        void InitPrefession();
        void InitTable();
        void CalcMatchLimit();
        int32_t CanMatch(const MatchInfo & member, const MatchInfo & rmember)const;
        int32_t CheckTeamProfessionId(int32_t professionId)const;
    private:
        pb_player_list_type m_vMyPlayers;
        uint64_t uint_id_{0};
        int32_t elo_id_{ 0 };
        i32_map_type m_vUnitPrefessions;
        static Snowflake m_oSnowflake;
        Team::team_id_type m_nTeamId{ Team::EmptyTeamId };
        TeamMemberInfo m_oTeamMemberInfo;
        MatchLimit m_oMatchLimit;
        const MatchElement * m_pMatchElement{ NULL };
        int32_t m_nMatchType{ 0 };
        MatchTargetInfo m_oMatchTarget;
        TeamTarget m_oTeamTarget;
        INT m_nTeamType{Team::E_TEAM_TYPE };
        const TeamMatchingElement * m_pTeamMatchElement{ nullptr };
		bool m_SkipProfession = false;
        TeamList * p_team_list_{ nullptr };
        int32_t begin_time_{0};
    };
}//namespace GameMMR

#endif // __MATCH_PALYER_H_

