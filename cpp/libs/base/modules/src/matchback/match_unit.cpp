#include "MatchUnit.h"
#include "GenCode/GameDefine_Result.h"

#include "CommonLogic/Team/TeamList.h"
#include "GenCode/Config/MatchCfg.h"
#include "GenCode/Config/TeamMatchingCfg.h"
#ifdef __TEST__
#include <iostream>
#endif // __TEST__

namespace GameMMR
{
    void TeamMemberToMatchInfo(const TeamMemberInfo& ti, MatchInfo& mi)
    {
        mi.set_roleid(ti.roleid());
        mi.set_level(ti.level());
        mi.set_name(ti.name());
        mi.set_configid(ti.configid());
        mi.set_professionid(ti.professionid());
        mi.set_guild(ti.guildguid());
        mi.set_ghostguid(ti.ghostguid());
        mi.set_robot(ti.robot());
        mi.mutable_pinchdata()->CopyFrom(ti.pinchdata());
        mi.mutable_treasureinfo()->CopyFrom(ti.treasureinfo());
        mi.set_battlescore(ti.battlescore());
        mi.set_hp(ti.hp());
        mi.mutable_location()->CopyFrom(ti.location());
        mi.set_followteam(ti.followteam());
        mi.set_headid(ti.headid());
        mi.set_headframeid(ti.headframeid());
    }

    void MatchInfoToTeamMember(const MatchInfo& mi, TeamMemberInfo& ti)
    {
        ti.set_roleid(mi.roleid());
        ti.set_level(mi.level());
        ti.set_name(mi.name());
        ti.set_configid(mi.configid());
        ti.set_professionid(mi.professionid());
        ti.set_guildguid(mi.guild());
        ti.set_ghostguid(mi.ghostguid());
        ti.set_robot(mi.robot());
        ti.mutable_pinchdata()->CopyFrom(mi.pinchdata());
        ti.mutable_treasureinfo()->CopyFrom(mi.treasureinfo());
        ti.set_battlescore(mi.battlescore());
        ti.set_hp(mi.hp());
        ti.mutable_location()->CopyFrom(mi.location());
        ti.set_followteam(mi.followteam());
        ti.set_headid(mi.headid());
        ti.set_headframeid(mi.headframeid());
    }
    Snowflake MatchUnit::m_oSnowflake;
    const MatchUnit::player_integral_map_type MatchUnit::kEmptyMatchRankInfo;
    const MatchTargetInfo MatchUnit::kEmptyMatchTarget;

    MatchUnit::MatchUnit(int32_t elo_id,
        int32_t nMatchType,
        const TeamInfo & info,
        const player_integral_map_type & kRankInfo,
        const MatchTargetInfo & oMatchTarget
    )
        :
        elo_id_(elo_id),
        m_nMatchType(nMatchType),
        m_oMatchTarget(oMatchTarget)
    {
        OnInit();
        InitTable();
        uint_id_ = m_oSnowflake.generate(uint_id_);
        m_nTeamId = info.teamid();
        m_oTeamTarget = info.teamtarget();
        m_nTeamType = info.teamtype();
        TeamToMatchInfo(info, m_vMyPlayers);
        SetRankInfo(kRankInfo);
        InitPrefession();
        CalcMatchLimit();
    }

    MatchUnit::MatchUnit(int32_t elo_id,
        int32_t nMatchType,
        const  MatchInfo & info,
        const player_integral_map_type & kRankInfo,
        const MatchTargetInfo & oMatchTarget)
        :
        elo_id_(elo_id),
        m_nMatchType(nMatchType),
        m_nTeamId(Team::EmptyTeamId),
        m_oMatchTarget(oMatchTarget)
    {
        OnInit();
        m_nTeamType = 0;
        InitTable();
        uint_id_ = m_oSnowflake.generate(uint_id_);
        m_vMyPlayers.emplace(info.roleid(), info);

        MatchInfoToTeamMember(info, m_oTeamMemberInfo);
        //m_oTeamMemberInfo.set_jointeamway(E_NOMRAL_JOIN);
        m_vMyPlayers.emplace(info.roleid(), info);

        SetRankInfo(kRankInfo);
        InitPrefession();
        CalcMatchLimit();
    }

    MatchUnit::MatchUnit(Team::team_id_type team_id, TeamList * p_team_list)
        : MatchUnit(0, E_NOMAL_TEAM_MATCH, p_team_list->GetTeamInfo(team_id))
    {
    }

    void MatchUnit::OnInit()
    {
#ifndef __TEST__
        begin_time_ = g_pTimeManager->CurrentTimeS();
#endif // !__TEST__
    }

    bool MatchUnit::HasPlayer(GUID_t palyerId)
    {
        return m_vMyPlayers.find(palyerId) != m_vMyPlayers.end();
    }

    void MatchUnit::SetProfession(GUID_t roleId, int32_t p)
    {
        pb_player_list_type::iterator it = m_vMyPlayers.find(roleId);
        if (it == m_vMyPlayers.end())
        {
            return;
        }

        it->second.set_professionid(p);

        InitPrefession();
    }

    void MatchUnit::UpdatePlayer(MatchInfo & mi)
    {
        pb_player_list_type::iterator it = m_vMyPlayers.find(mi.roleid());
        if (it == m_vMyPlayers.end())
        {
            return;
        }
        it->second.CopyFrom(mi);
    }

    void MatchUnit::InitPrefession()
    {
        m_vUnitPrefessions.clear();
        for (auto & it : m_vMyPlayers)
        {
            i32_map_type::iterator pIt = m_vUnitPrefessions.find(it.second.professionid());
            if (pIt != m_vUnitPrefessions.end())
            {
                ++pIt->second;
            }
            else
            {
                m_vUnitPrefessions.emplace(it.second.professionid(), 1);
            }
        }
    }

    void MatchUnit::InitTable()
    {
        m_pMatchElement = MatchTable::Instance().GetElement(m_nMatchType);
        m_pTeamMatchElement = TeamMatchingTable::Instance().GetElement(elo_id_);
    }

    int32_t MatchUnit::JoinPlayer(match_unit_ptr_type & rhf)
    {
        for (auto && it : rhf->GetPlayers())
        {
            m_vMyPlayers.emplace(it.first, it.second);
        }
        i32_map_type & v = rhf->GetProfession();
        for (i32_map_type::iterator it = v.begin(); it != v.end(); ++it)
        {
            i32_map_type::iterator myProfessionIt = m_vUnitPrefessions.find(it->first);
            if (myProfessionIt != m_vUnitPrefessions.end())
            {
                myProfessionIt->second += it->second;
            }
            else
            {
                m_vUnitPrefessions.emplace(it->first, 1);
            }
        }
        return OR_OK;
    }

    void MatchUnit::CalcMatchLimit()
    {
        if (NULL == m_pMatchElement)
        {
            return;
        }

        if (m_vMyPlayers.empty())
        {
            return;
        }
        if (IsSingle())
        {
            m_oMatchLimit.set_minlevel(m_vMyPlayers.begin()->second.level());
            m_oMatchLimit.set_maxlevel(m_vMyPlayers.begin()->second.level());
            m_oMatchLimit.set_minintegral(m_vMyPlayers.begin()->second.integral());
            m_oMatchLimit.set_maxintegral(m_vMyPlayers.begin()->second.integral());

            int32_t minSection = m_vMyPlayers.begin()->second.section() - m_pMatchElement->extern_section;
            if (!m_pMatchElement->sections_list.empty())
            {
                minSection = minSection >= *m_pMatchElement->sections_list.begin() ? minSection : *m_pMatchElement->sections_list.begin();
            }
            m_oMatchLimit.set_minsection(minSection);

            int32_t maxSection = m_vMyPlayers.begin()->second.section() + m_pMatchElement->extern_section;
            if (!m_pMatchElement->sections_list.empty())
            {
                int32_t maxTableSection = m_pMatchElement->sections_list[m_pMatchElement->sections_list.size() - 1];
                maxSection = maxSection <= maxTableSection ? maxSection : maxTableSection;
            }
            m_oMatchLimit.set_maxsection(maxSection);
            return;
        }

        int32_t i = 0;
        for (pb_player_list_type::iterator it = m_vMyPlayers.begin(); it != m_vMyPlayers.end(); ++it, ++i)
        {
            if (i == 0)
            {
                m_oMatchLimit.set_minlevel(it->second.level());
                m_oMatchLimit.set_maxlevel(it->second.level());
                m_oMatchLimit.set_minintegral(it->second.integral());
                m_oMatchLimit.set_maxintegral(it->second.integral());
                m_oMatchLimit.set_minsection(it->second.section());
                m_oMatchLimit.set_maxsection(it->second.section());
                continue;
            }
            m_oMatchLimit.set_minlevel(std::min(m_oMatchLimit.minlevel(), it->second.level()));
            m_oMatchLimit.set_maxlevel(std::min(m_oMatchLimit.maxlevel(), it->second.level()));
            m_oMatchLimit.set_minintegral(std::min(m_oMatchLimit.minintegral(), it->second.integral()));
            m_oMatchLimit.set_maxintegral(std::max(m_oMatchLimit.maxintegral(), it->second.integral()));
            m_oMatchLimit.set_minsection(std::min(m_oMatchLimit.minsection(), it->second.section()));
            m_oMatchLimit.set_maxsection(std::max(m_oMatchLimit.maxsection(), it->second.section()));
        }
    }

    void MatchUnit::PlayerCancel(GUID_t palyerId)
    {
        m_vMyPlayers.erase(palyerId);
        InitPrefession();
    }

    bool MatchUnit::IsEmpty()
    {
        return m_vMyPlayers.empty();
    }

    int32_t MatchUnit::CanMatch(MatchUnit & r)
    {
        if (m_vMyPlayers.empty())
        {
            return OR_MATCH_PLAYER_LIST_ERROR;
        }
        if (r.GetPlayers().empty())
        {
            return OR_MATCH_RPLAYER_LIST_ERROR;
        }
        if (IsSingle() && r.IsSingle())
        {
            return CanMatch(m_vMyPlayers.begin()->second, r.GetPlayers().begin()->second);
        }
        else if (IsTeamGroup() && r.IsSingle())
        {
            for (pb_player_list_type::const_iterator it = m_vMyPlayers.begin(); it != m_vMyPlayers.end(); ++it)
            {
                int32_t nRet = CanMatch(it->second, r.GetPlayers().begin()->second);
                if (OR_OK != nRet)
                {
                    return nRet;
                }
            }
        }
        else if (r.IsTeamGroup() && IsSingle())
        {
            for (auto && it : r.GetPlayers())
            {
                int32_t nRet = CanMatch(m_vMyPlayers.begin()->second, it.second);
                if (OR_OK != nRet)
                {
                    return nRet;
                }
            }
        }
        else if (r.IsTeamGroup() && IsTeamGroup())
        {
            for (auto && it : r.GetPlayers())
            {
                for (auto && ji : GetPlayers())
                {
                    int32_t nRet = CanMatch(ji.second, it.second);
                    if (OR_OK != nRet)
                    {
                        return nRet;
                    }
                }
            }
        }
        return OR_OK;
    }

    int32_t MatchUnit::CanMatch()
    {
        if (IsSingle())
        {
            return OR_OK;
        }

        if (m_oMatchLimit.maxsection() - m_oMatchLimit.minsection() > m_pMatchElement->team_section_span)
        {
            return OR_MATCH_TEAM_SECTION_ERROR;
        }

        return OR_OK;
    }

    int32_t MatchUnit::CanMatch(const MatchInfo & myMember, const MatchInfo & otherMember)const
    {
        if (NULL == m_pMatchElement)
        {
            return OR_MATCH_UNIT_TABLE_ERROR;
        }

        if (std::abs(myMember.section() - otherMember.section()) > m_pMatchElement->team_section_span)
        {
            return OR_MATCH_SECTION_SPAN_ERROR;
        }

        if (IsTeamGroup())
        {
            if (otherMember.level() < m_oTeamTarget.minlevel() || otherMember.level() > m_oTeamTarget.maxlevel())
            {
                return OR_MATCH_MATCH_TEAM_LEVEL_ERROR;
            }
            if (!m_SkipProfession)
            {
                OR_CHECK_RESULT_RETURN_RET(CheckTeamProfessionId(otherMember.professionid()));
            }
        }

        if (nullptr != m_pTeamMatchElement && m_pTeamMatchElement->alliance == E_MATCH_FILTER_GUILD)
        {
            if (myMember.guild() <= 0 || otherMember.guild() <= 0)
            {
                return OR_MATCH_DIFFRENCE_GUILD;
            }
            if (myMember.guild() != otherMember.guild())
            {
                return OR_MATCH_DIFFRENCE_GUILD;
            }
        }

        return OR_OK;
    }

    bool MatchUnit::operator < (const  MatchUnit & r)const
    {
        MatchLimit roMatchLimit = r.GetMatchLimit();

        if (m_oMatchLimit.minsection() < roMatchLimit.minsection())
        {
            return true;
        }

        if (m_oMatchLimit.maxsection() < roMatchLimit.maxsection())
        {
            return true;
        }

        if (GetAvergeIntegral() < r.GetAvergeIntegral())
        {
            return true;
        }

        if (GetPlayersSize() < r.GetPlayersSize())
        {
            return true;
        }
        else
        {
            return false;
        }

        return false;
    }

    int32_t MatchUnit::CheckTeamProfessionId(int32_t professionId)const
    {
        if (m_oTeamTarget.profession_size() <= 0)
        {
            return OR_OK;
        }

        bool fullProfession = true;

        int32_t needSize = 0;

        i32_map_type vNeedPrefessions;

        for (int32_t i = 0; i < m_oTeamTarget.profession_size(); ++i)
        {
            int32_t proId = m_oTeamTarget.profession(i).professionid();
            int32_t proSize = m_oTeamTarget.profession(i).size();
            i32_map_type::const_iterator it = m_vUnitPrefessions.find(proId);
            if (it == m_vUnitPrefessions.end())
            {
                fullProfession = false;
                needSize += proSize;
                vNeedPrefessions.emplace(proId, proSize);
                continue;
            }
            if (it->second < proSize)
            {
                fullProfession = false;
                needSize += proSize - it->second;
                vNeedPrefessions.emplace(proId, proSize - it->second);
                continue;
            }
        }

        //个数够了怎么加都行
        if (fullProfession)
        {
            return OR_OK;
        }

        if (Team::GetMaxSize(GetTeamType()) - m_vMyPlayers.size() > (size_t)needSize)
        {
            return OR_OK;
        }

        //需求里面的职业直接通过
        for (auto && it : vNeedPrefessions)
        {
            if (it.first == professionId)
            {
                return OR_OK;
            }
        }
        return OR_MATCH_PROFESSION_ERROR;
    }

    void MatchUnit::TestOutPut()
    {
#ifdef __TEST__
        std::cout << "section:" << (m_oMatchLimit.maxsection() - m_oMatchLimit.minsection()) / GetPlayersSize() << std::endl;
#endif//__TEST__
    }

    int32_t MatchUnit::GetAvergeIntegral()const
    {
        if (m_vMyPlayers.empty())
        {
            return 1;
        }

        int32_t nIntegral = 0;
        for (auto && it : m_vMyPlayers)
        {
            nIntegral += it.second.integral();
        }

        return nIntegral / (int32_t)m_vMyPlayers.size();
    }
}//namespace GameMMR