#include "Team.h"
#include "GenCode/GameDefine_Result.h"
#include "GenCode/Config/SceneCfg.h"
#include "GenCode/Config/GlobalCfg.h"
#include "CommonLogic/EventStruct/TeamEventStruct.h"


const TeamTarget Team::kTeamTarget;
static const GUID_t kEmptyRoleId = 0;

Team::Team(const CreateTeamParam& param)
    : 
    m_nTeamId(param.nTeamId),
    m_nLeaderId(param.nLeaderId),
    m_nTeamType(param.nTeamType),
    m_oTarget(param.target)
{
    emid(param.emid_);
    for (auto& it : param.v)
    {
        OnJoinTeam(it.second);
    }
}

INT Team::AgreeApplicant(GUID_t nApplyRoleId)
{
    applymap_type::iterator it = m_vApplyMembers.find(nApplyRoleId);
    if (it == m_vApplyMembers.end())
    {
        return OR_NOTIN_TEAM_APPLY_LIST;
    }

    TeamMemberInfo m;
    m.CopyFrom(it->second);
    INT nRet = JoinTeam(m);
    if (nRet != OR_OK)
    {
        return nRet;
    }
    nRet = RemoveApply(nApplyRoleId);
    return OR_OK;
}

void Team::CopyFrom(TeamMemberInfo & from)
{
    membermap_type::iterator mit = m_vMembers.find(from.roleid());
    if (mit == m_vMembers.end())
    {
        return;
    }
    if (!from.has_followteam())
    {
        from.set_followteam(mit->second.followteam());
    }
    if (mit->second.followmemberplayerid() > 0)
    {
        from.set_followmemberplayerid(mit->second.followmemberplayerid());
    }    
    mit->second.CopyFrom(from);
}

INT Team::JoinTeam(const TeamMemberInfo & m)
{
    OR_CHECK_RESULT(TryToJoinTeam(m));
    OnJoinTeam(m);
    return OR_OK;
}

INT Team::TryToJoinTeam(const TeamMemberInfo & m)
{
    if (m.roleid() == 0 || m.roleid() == INVALID_ID)
    {
        return OR_TEAM_JOIN_TEAM_ROLE_ID_ERROR;
    }

    if (IsFull())
    {
        return OR_TEAM_MEM_FULL;
    }
    membermap_type::iterator it = m_vMembers.find(m.roleid());
    if (it != m_vMembers.end())
    {
        return OR_TEAM_MEM_INTEAM;
    }
    return OR_OK;
}

GUID_t Team::ReplaceRobot(TeamMemberInfo & m)
{
	GUID_t roleid = 0;
	if (IsFull())
	{
		// 尝试替换机器人
		for (membermap_type::iterator it = m_vMembers.begin(); it != m_vMembers.end(); ++it)
		{
			if (it->second.robot() && it->second.professionid() == m.professionid())
			{
				roleid = it->second.roleid();
				break;
			}
		}
	}
	return roleid;
}

void Team::OnJoinTeam(const TeamMemberInfo & m)
{
    m_vMembers.emplace(m.roleid(), m);
    m_vRoleIdList.push_back(m.roleid());
    RemoveApply(m.roleid());

    if ( !m_vMembers.empty())
    {
        emc::i().emit<TeamEventES>(emid(), E_JOIN_TEAM, m_nTeamId, m.roleid());

    }
    else if (m_vMembers.empty())
    {
        emc::i().emit<TeamEventES>(emid(), E_CREATE_TEAM_JOIN_TYPE, m_nTeamId, m.roleid());


    }

#ifndef __TEST__
 /*   auto p_team_table = TeamMatchingTable::Instance().GetElement(m_oTarget.id());
    if (nullptr != p_team_table)
    {
        if (p_team_table->team_fallow > 0)
        {
            SetFollowState(m.roleid(), E_FOLLOW);
        }
        else
        {
            SetFollowState(m.roleid(), E_FREE);
        }
    }*/
#else
    SetFollowState(m.roleid(), E_FOLLOW);
#endif//__TEST__

    if (IsLeader(m.roleid()))
    {
        SetFollowState(m.roleid(), E_FOLLOW);
    }
    
	if (IS_ROBOT_TYPE(m.roleid()))
	{
		SetFollowState(m.roleid(), E_FOLLOW);
	}
}

INT Team::LeaveTeam(GUID_t nRoleId)
{
    membermap_type::iterator it = m_vMembers.find(nRoleId);
    if (it == m_vMembers.end())
    {
        return OR_NOT_IN_TEAM;
    }
    //change leader
   if (IsLeader(nRoleId))
   {
       ReElectTeamLeader(nRoleId);
   }

    OnLeaveTeam(nRoleId);
    OnEraseRole(nRoleId);
    return OR_OK;
}

void Team::ReElectTeamLeader(GUID_t nRoleId)
{
    if (!HasMember(nRoleId))
    {
        return;
    }
    auto old_leader = m_nLeaderId;

	GUID_t new_leader = old_leader;

#ifndef __TEST__

    bool all_off_line = true;
    for (auto && it : m_vMembers)
    {
        if (!it.second.robot()
            && it.second.online() == E_ON_LINE
            && nRoleId != it.second.roleid())
        {
            all_off_line = false;
        }
    }

    if (!all_off_line && IsLeader(nRoleId) && InBaseScene(GetMemberInfo(nRoleId)))
    {
        bool leader_change = false;
        GUID_t max_level_member = m_nLeaderId;
        for (auto && it : m_vMembers)
        {
            if (!it.second.robot()
                && InBaseScene(it.second)
                && it.second.online() == E_ON_LINE
                && nRoleId != it.second.roleid())
            {
				new_leader = it.second.roleid();
                leader_change = true;
                break;
            }
        }
        if (!leader_change)
        {
            for (auto && it : m_vMembers)
            {
                if (!it.second.robot()
                    && NotInBaseScene(it.second)
                    && it.second.online() == E_ON_LINE
                    && nRoleId != it.second.roleid())
                {
					new_leader = it.second.roleid();
                    leader_change = true;
                    break;
                }
            }
        }
    }
    else if (!all_off_line && IsLeader(nRoleId) && NotInBaseScene(GetMemberInfo(nRoleId)))
    {
        bool leader_change = false;
        GUID_t max_level_member = m_nLeaderId;
        for (auto && it : m_vMembers)
        {
            if (!it.second.robot()
                && NotInBaseScene(it.second)
                && it.second.online() == E_ON_LINE
                && nRoleId != it.second.roleid())
            {
				new_leader = it.second.roleid();
                leader_change = true;
                break;
            }
        }
        if (!leader_change)
        {
            for (auto && it : m_vMembers)
            {
                if (!it.second.robot()
                    && InBaseScene(it.second)
                    && it.second.online() == E_ON_LINE
                    && nRoleId != it.second.roleid())
                {
					new_leader = it.second.roleid();
                    leader_change = true;
                    break;
                }
            }
        }
    }
    else if(all_off_line)
    {
        for (auto&it : m_vRoleIdList)
        {
            if (it == m_nLeaderId || IS_ROBOT_TYPE(it))
            {
                continue;
            }

			new_leader = it;
            break;
        }
    }
#else
    if (!m_vRoleIdList.empty() && IsLeader(nRoleId))
    {
        for (auto&it : m_vRoleIdList)
        {
            if (it == m_nLeaderId)
            {
                continue;
            }

			new_leader = it;
            break;
        }
    }
#endif // !__TEST__

    if (new_leader > 0 && old_leader != new_leader )
    {
        OnAppointLeader(new_leader);
    }
}

void Team::OnLeaveTeam(GUID_t nRoleId)
{

    if (IsLeader(nRoleId))
    {
        emc::i().emit<TeamEventES>(emid(), E_BEFORE_LEADER_LEAVE_TEAM, m_nTeamId, nRoleId);
    }
    else if (!IsLeader(nRoleId))
    {
        emc::i().emit<TeamEventES>(emid(), E_BEFORE_MEMBER_LEAVE_TEAM, m_nTeamId, nRoleId);
    }

    emc::i().emit<TeamEventES>(emid(), E_BEFORE_LEAVE_TEAM_TYPE, m_nTeamId, nRoleId);
 
    m_vMembers.erase(nRoleId);
    roleid_list_type::iterator idit = std::find(m_vRoleIdList.begin(), m_vRoleIdList.end(), nRoleId);
    if (idit != m_vRoleIdList.end())
    {
        m_vRoleIdList.erase(idit);
    }
}


void Team::Logout(GUID_t nRoleId)
{
    membermap_type::iterator it = m_vMembers.find(nRoleId);
    if (it == m_vMembers.end())
    {
        return;
    }
	if (!IS_ROBOT_TYPE(nRoleId))
		it->second.set_online(E_OFF_LINE);
	
}

void Team::Login(GUID_t nRoleId)
{
    membermap_type::iterator it = m_vMembers.find(nRoleId);
    if (it == m_vMembers.end())
    {
        return;
    }

    it->second.set_online(E_ON_LINE);
}

INT Team::KickMem(GUID_t nCurLeaderId, GUID_t  nKickRoleId)
{
    if (m_nLeaderId == nKickRoleId)
    {
        return OR_KICK_LEADERSELF;
    }

    if (m_nLeaderId != nCurLeaderId)
    {
        return OR_NOT_LEADER;
    }

    membermap_type::iterator it = m_vMembers.find(nKickRoleId);
    if (it == m_vMembers.end())
    {
        return OR_NOT_IN_TEAM;
    }
    LeaveTeam(nKickRoleId);
    return OR_OK;
}

INT Team::AppointLeader(GUID_t nCurLeaderId, GUID_t nNewLeaderRoleId)
{
	if (m_nLeaderId == nNewLeaderRoleId)
	{
		return OR_APPOINT_LEADERSELF;
	}

    if (!HasMember(nNewLeaderRoleId))
    {
        return OR_NOT_IN_TEAM;
    }

	if (m_nLeaderId != nCurLeaderId)
	{
		return OR_NOT_LEADER;
	}

    if (IsRobot(nNewLeaderRoleId))
    {
        return OR_TEAM_MEMBER_IS_ROBOT;
    }

	OnAppointLeader(nNewLeaderRoleId);
	return OR_OK;
}

void Team::OnAppointLeader(GUID_t nNewLeaderRoleId)
{
    if (nNewLeaderRoleId <= 0)
    {
        return;
    }
    SetLeaderId(nNewLeaderRoleId);
    emc::i().emit<TeamEventES>(emid(), E_APPOINT_LEADER, m_nTeamId, nNewLeaderRoleId);
    SetFollowState(nNewLeaderRoleId, E_FOLLOW);

}

INT Team::Apply(const TeamMemberInfo & m)
{
    if (IsFull())
    {
        return OR_TEAM_MEM_FULL;
    }

    if (m_vMembers.find(m.roleid()) != m_vMembers.end())
    {
        return OR_TEAM_MEM_INTEAM;
    }

    OR_CHECK_RESULT(CheckLimt(m));

    applymap_type::iterator it = m_vApplyMembers.find(m.roleid());
    if (it != m_vApplyMembers.end())
    {
        return OR_IN_TEAM_APPLY_LIST;
    }
    if (m_vApplyRoleIdList.size() >= MAX_APPLY)
    {
#ifdef __TEST__
		syassertn(!m_vApplyRoleIdList.empty());
#endif // __TEST__

        m_vApplyMembers.erase(*m_vApplyRoleIdList.begin());
        m_vApplyRoleIdList.erase(m_vApplyRoleIdList.begin());
    }
	m_vApplyMembers.emplace(m.roleid(), m);
	if (!m.robot())
	{
		m_vApplyRoleIdList.push_back(m.roleid());
	}
    return OR_OK;
}

GUID_t Team::FirstApplyRoleId()
{
    if (m_vApplyRoleIdList.empty())
    {
        return 0;
    }
    return *m_vApplyRoleIdList.begin();
}


INT Team::RemoveApply(GUID_t nApplyRoleId)
{
    applymap_type::iterator it = m_vApplyMembers.find(nApplyRoleId);
    if (it == m_vApplyMembers.end())
    {
        return OR_NOTIN_TEAM_APPLY_LIST;
    }

    m_vApplyMembers.erase(nApplyRoleId);

    roleid_list_type::iterator idit = std::find(m_vApplyRoleIdList.begin(), m_vApplyRoleIdList.end(), nApplyRoleId);
    if (idit != m_vApplyRoleIdList.end())
    {
        m_vApplyRoleIdList.erase(idit);
    }
    OnEraseRole(nApplyRoleId);
    return OR_OK;
}

INT Team::Invite(TeamMemberInfo & oBeInvite)
{
    if (IsFull())
    {
        return OR_TEAM_MEM_FULL;
    }
    if (m_vInvitations.size() >= MAX_INVITATION)
    {
        m_vInvitations.pop_front();
    }
    m_vInvitations.push_back(oBeInvite);
    return OR_OK;
}


INT Team::AgreeInvite(GUID_t nInvite)
{
    for (invitationids_type::iterator it = m_vInvitations.begin(); it != m_vInvitations.end(); ++it)
    {
        if (it->roleid() == nInvite)
        {

            INT nRet = JoinTeam(*it);
            m_vInvitations.erase(it);
            return nRet;
        }
    }

    return OR_NOT_INVITATION_MEMBER;
}



int32_t Team::SetTarget(GUID_t nRoleId, const TeamTarget & target)
{

    if (!HasMember(nRoleId))
    {
        return OR_NOT_IN_TEAM;
    }
    if (target.profession_size() > 3)
    {
        return OR_TEAM_FUNCTION_SIZE;
    }

    for (int32_t i = 0; i < target.profession_size(); ++i)
    {
        if (target.profession(i).professionid() < 0)
        {
            return OR_TEAM_FUNCTION_ERROR;
        }
    }


    if (!IsLeader(nRoleId))
    {
        return OR_NOT_LEADER;
    }
  
    if (target.targettype() < 0 )
    {
        if (target.targettype() == E_TEAM_NO_TARGET ||
            target.targettype() == E_TEAM_SURROUANDING ||
            target.targettype() == E_TEAM_TARGET_TYPE_ALL )
        {
        }
    }

    m_oTarget.CopyFrom(target);


    emc::i().emit<TeamEventES>(emid(), E_CHANGE_TARGET_TYPE, m_nTeamId, nRoleId);

    return OR_OK;
}


int32_t Team::CopyTarget(const TeamTarget& target)
{
    m_oTarget.CopyFrom(target);
    return OR_OK;
}

int32_t Team::OnChangeName(GUID_t role_id, const std::string& n)
{
    membermap_type::iterator it = m_vMembers.find(role_id);
    if (it == m_vMembers.end())
    {
        return OR_NOT_IN_TEAM;
    }
    it->second.set_name(n);
    emc::i().emit<TeamEventES>(emid(), E_MEMBER_NAME_CHANGE_TTYPE, m_nTeamId, role_id);
    return OR_OK;
}

int32_t Team::CheckLimt(const TeamMemberInfo & m)
{
    if (m_oTarget.minlevel() == m_oTarget.maxlevel() && m_oTarget.minlevel() <= 0)
    {
        return OR_OK;
    }

    if (m.level() < m_oTarget.minlevel() || m.level() > m_oTarget.maxlevel())
    {
        return OR_TEAM_LEVEL_LIMIT_ERROR;
    }

    return OR_OK;
}


int32_t Team::ChangeTeamType(GUID_t nRoleId, int32_t ntype)
{
    if (!HasMember(nRoleId))
    {
        return OR_NOT_IN_TEAM;
    }
    if (!IsLeader(nRoleId))
    {
        return OR_NOT_LEADER;
    }
    if (ntype < Team::E_TEAM_TYPE || ntype >= Team::E_TEAM_TYPE_MAX)
    {
        return OR_TEAM_TYPE;
    }

   
    if (m_nTeamType == E_GROUP_TYPE)
    {
        return OR_TEAM_TO_TYPE_ERROR;
    }
    m_nTeamType = ntype;
    int32_t min_level = m_oTarget.minlevel();
    int32_t max_level = m_oTarget.maxlevel();
    m_oTarget = kTeamTarget;
    if (m_nTeamType == E_GROUP_TYPE)
    {
        //转成团队换成全部类型
        m_oTarget.set_id(0);
        m_oTarget.set_targettype(E_TEAM_NO_TARGET);
        m_oTarget.set_minlevel(min_level);
        m_oTarget.set_maxlevel(max_level);
        emc::i().emit<TeamEventES>(emid(), E_CHANGE_TEAM_TYPE, m_nTeamId, nRoleId);
    }

    return OR_OK;
}


void Team::SetTeamType(int32_t teamType)
{

    if (teamType < Team::E_TEAM_TYPE || teamType >= Team::E_TEAM_TYPE_MAX)
    {
        return;
    }

    m_nTeamType = teamType;
}

int32_t Team::GetMemberMinLevel()
{
    int32_t minLevel = 10000000;
    for (membermap_type::iterator it = m_vMembers.begin(); it != m_vMembers.end(); ++it)
    {
        if (minLevel > it->second.level())
        {
            minLevel = it->second.level();
        }
    }
    return minLevel;
}

int32_t Team::GetMemberMaxLevel()
{
    int32_t maxLevel = 0;
    for (membermap_type::iterator it = m_vMembers.begin(); it != m_vMembers.end(); ++it)
    {
        if (maxLevel < it->second.level())
        {
            maxLevel = it->second.level();
        }
    }
    return maxLevel;
}

int32_t Team::GetMemberAverageLevel()
{
    if (m_vMembers.empty())
    {
        return 0;
    }
    int32_t aLevel = 0;
    for (membermap_type::iterator it = m_vMembers.begin(); it != m_vMembers.end(); ++it)
    {
        aLevel += it->second.level();
    }

    return aLevel / (int32_t)m_vMembers.size();
}

int32_t Team::GetMemberMinBattleScore()
{
	if (m_vMembers.empty())
	{
		return 0;
	}

	auto iter = m_vMembers.begin();
	int32_t nMinBattelScore = iter->second.battlescore();

	for (auto iter0 : m_vMembers)
	{
		int32_t nTempBattleScore = iter0.second.battlescore();
		if (nTempBattleScore < nMinBattelScore)
		{
			nMinBattelScore = nTempBattleScore;
		}
	}

	return nMinBattelScore;
}


int32_t Team::GetMemberAverageBattleScore()
{
	if (m_vMembers.empty())
	{
		return 0;
	}
	int32_t nBattleScore = 0;
	auto iter = m_vMembers.begin();
	for (auto iter : m_vMembers)
	{
		nBattleScore += iter.second.battlescore();
	}
	nBattleScore /= m_vMembers.size();
	return nBattleScore;
}

int32_t Team::GetMemberAverageLevelWithOutRobot()
{
	if (m_vMembers.empty())
	{
		return 0;
	}
	int32_t aLevel = 0;
	int32_t nTotal = 0;
	for (membermap_type::iterator it = m_vMembers.begin(); it != m_vMembers.end(); ++it)
	{
		if(IS_ROBOT_TYPE(it->second.configid()))
			continue;
		aLevel += it->second.level();
		nTotal ++;
	}

	if (nTotal == 0)
		return 0;
	return aLevel / nTotal;
}

void Team::ToTeamTarget(TeamTarget & target)
{
    target.CopyFrom(m_oTarget);
}

void Team::GetTeamApplyList(::google::protobuf::RepeatedPtrField< ::TeamMemberInfo >*pv)
{
    pv->Clear();

    for (roleid_list_type::iterator it = m_vApplyRoleIdList.begin(); it != m_vApplyRoleIdList.end(); ++it)
    {
        auto && jt = m_vApplyMembers.find(*it);
        if (m_vApplyMembers.find(*it) != m_vApplyMembers.end())
        {
            pv->Add()->CopyFrom(jt->second);
        }
    }
   
}


void Team::ClearApplyList()
{
    m_vApplyMembers.clear();
    m_vApplyRoleIdList.clear();
}

bool Team::SameTarget(int32_t teamgroup, int32_t teamgroupid)
{
    if (teamgroup == m_oTarget.targettype( ) && teamgroupid == m_oTarget.id() )
    {
        return true;
    }

    return false;
}


bool Team::HasMemberNotInBaseScene()
{
    for (auto && it : m_vMembers)
    {
		if(IS_ROBOT_TYPE(it.first))
			continue;
        if (NotInBaseScene(it.second))
        {
            return true;
        }
    }
    return false;
}

bool Team::NotInBaseScene(const TeamMemberInfo & m)
{
    return !InBaseScene(m);
}

bool Team::NotInBaseScene(GUID_t m)
{
    return !IsInBaseScene(m);
}

bool Team::IsInBaseScene(GUID_t roleId)
{
    membermap_type::iterator it = m_vMembers.find(roleId);
    if (it == m_vMembers.end())
    {
        return false;
    }

    return InBaseScene(it->second);
}

bool Team::InBaseScene(const TeamMemberInfo & m)
{
   

#ifndef __TEST__
    if (m.location().sceneconfigid() > 0)
    {
        const GlobalElement* p_ele = GlobalTable::Instance().GetElement(41);
        if (nullptr != p_ele)
        {
            const SceneElement * pSceneElement = SceneTable::Instance().GetElement(m.location().sceneconfigid());

            if (nullptr == pSceneElement)
            {
                return false;
            }

            for (auto& it : p_ele->list_int_value)
            {
                if (it == pSceneElement->scene_type)
                {
                    return false;
                }
            }
        }
    }
#endif//__TEST__

    if (m.location().dungeonconfigid() > 0)
    {
#ifndef __TEST__
       const GlobalElement* p_ele = GlobalTable::Instance().GetElement(42);
       if (nullptr != p_ele)
       {
           for (auto& it : p_ele->list_int_value)
           {
               if (it == m.location().dungeonconfigid())
               {
                   return true;
               }
           }
       }
#endif//__TEST__
        return false;
    }
    if (m.location().mirrorid() > 0)
    {
        return false;
    }
    return true;
}

bool Team::InMirrorScene(const TeamMemberInfo & m)
{
 
    if (m.location().mirrorid() > 0)
    {
        return true;
    }
    return false;
}

bool Team::IsMemberDie()
{
	for (auto iter0 : m_vMembers)
	{
		if (0 >= iter0.second.hp())
		{
			return true;
		}
	}

	return false;
}


void Team::OnCreate()
{
    for (auto& it : m_vMembers)
    {
        emc::i().emit<TeamEventES>(emid(), E_CREATE_TEAM_TYPE, m_nTeamId, it.first);
   
    }
}

bool Team::HasMember(GUID_t player_guid)
{
    return m_vMembers.find(player_guid) != m_vMembers.end();
}

int32_t Team::FollowMember(GUID_t player_id, GUID_t be_follow_player_id)
{
    membermap_type::iterator it = m_vMembers.find(player_id);
    if (it == m_vMembers.end())
    {
        return OR_NOT_IN_TEAM;
    }
    membermap_type::iterator it_be = m_vMembers.find(be_follow_player_id);
    if (it_be == m_vMembers.end())
    {
        return OR_NOT_IN_TEAM;
    }
    if (it_be->second.followmemberplayerid() == player_id)
    {
        return OR_TEAM_FOLLOW_EACH_OTHER;
    }
    if (IsLeader(player_id) && IsFollow(be_follow_player_id))
    {
        return OR_TEAM_FOLLOW_EACH_OTHER;
    }
    it->second.set_followmemberplayerid(be_follow_player_id);

    emc::i().emit<TeamEventES>(emid(), E_FOLLOW_MEMBER, m_nTeamId, player_id);

    return OR_OK;
}

int32_t Team::CancelFollowMember(GUID_t roleId)
{
    membermap_type::iterator it = m_vMembers.find(roleId);
    if (it == m_vMembers.end())
    {
        return OR_NOT_IN_TEAM;
    }
    it->second.set_followmemberplayerid(kEmptyRoleId);

    emc::i().emit<TeamEventES>(emid(), E_CANCEL_FOLLOW_MEMBER, m_nTeamId, roleId);

    return OR_OK;
}

INT Team::UpdateTeamMemInfo(TeamMemberInfo & m)
{
	if (m.roleid() == 0)
	{
		return OR_TEAM_UPDATE_TEAM_INFO_ROLE_ID_ERROR;
	}

	membermap_type::iterator it = m_vMembers.find(m.roleid());
	if (it == m_vMembers.end())
	{
		return OR_NOT_IN_TEAM;
	}
	CopyFrom(m);
	return OR_OK;
}

INT Team::UpdateTeamMemInfoGs(TeamMemberInfo & m)
{
    if (m.roleid() == 0)
    {
        return OR_TEAM_UPDATE_TEAM_INFO_ROLE_ID_ERROR;
    }

    membermap_type::iterator it = m_vMembers.find(m.roleid());
    if (it == m_vMembers.end())
    {
        return OR_NOT_IN_TEAM;
    }
    it->second.CopyFrom(m);
    return OR_OK;
}

INT Team::GetTeamMemInfo(TeamMemberInfo & m)
{
	if (m.roleid() == 0)
	{
		return OR_TEAM_GET_TEAM_INFO_ROLE_ID_ERROR;
	}

	membermap_type::iterator it = m_vMembers.find(m.roleid());
	if (it == m_vMembers.end())
	{
		return OR_NOT_IN_TEAM;
	}

	m.CopyFrom(it->second);
	return OR_OK;
}

bool Team::IsFollow(GUID_t roleId)const
{
    membermap_type::const_iterator it = m_vMembers.find(roleId);
    if (it == m_vMembers.end())
    {
        return false;
    }
    return IsFollow(it->second);
}

bool Team::IsFollow(const TeamMemberInfo& m)
{
    return m.followteam() == E_FOLLOW && m.online() == E_ON_LINE;
}

bool Team::IsFollowMemberOrLeader(GUID_t roleId) const
{
    membermap_type::const_iterator it = m_vMembers.find(roleId);
    if (it == m_vMembers.end())
    {
        return false;
    }
    return !IsLeader(roleId) && (IsFollow(roleId) || it->second.followmemberplayerid() != kEmptyRoleId);
}

bool Team::IsRobot(GUID_t roleId)const
{
    membermap_type::const_iterator it = m_vMembers.find(roleId);
    if (it == m_vMembers.end())
    {
        return false;
    }
    return it->second.robot();
}

void Team::ClearRobotLocationData()
{
	membermap_type::iterator it = m_vMembers.begin();
	for (;it != m_vMembers.end();++it)
	{
		if (IS_ROBOT_TYPE(it->first))
		{
			it->second.clear_location();
		}
	}
}

int32_t Team::SetFollowState(GUID_t roleId, int32_t s)
{
    membermap_type::iterator it = m_vMembers.find(roleId);
    if (it == m_vMembers.end())
    {
        return OR_NOT_IN_TEAM;
    }
    if (s < E_FREE || s >  E_FOLLOW)
    {
        return OR_TEAM_FOLLOW_STATE_ERROR;
    }
    if (it->second.followteam() == s)
    {
        return OR_OK;
    }
    auto leaderit = m_vMembers.find(GetLeaderId());
    if (leaderit == m_vMembers.end())
    {
        return OR_TEAM_LEADER__FOLLOW_ERROR;
    }

    if (leaderit->second.followmemberplayerid() == roleId)
    {
        return OR_TEAM_FOLLOW_EACH_OTHER;
    }
    
    it->second.set_followteam(s);

    emc::i().emit<TeamEventES>(emid(), E_MEMBER_CHANGE_FOLLOW_STATE, m_nTeamId, roleId);

    return OR_OK;
}

int32_t Team::GetFollowState(GUID_t roleId)
{
    membermap_type::iterator it = m_vMembers.find(roleId);
    if (it == m_vMembers.end())
    {
        return E_FREE;
    }
    return it->second.followteam();
}

bool Team::IsAllMemberOffLine() const
{
    for (auto&it : m_vMembers)
    {
        if (IsRobot(it.first))
        {
            continue;
        }
        if (IsOnline(it.first))
        {
            return false;
        }
    }
    return true;
}

GUID_t Team::GetFirstMemberRoleId()
{
    if (m_vRoleIdList.empty())
    {
        return 0;
    }

    return *m_vRoleIdList.begin();
}

void Team::SetLeaderId(GUID_t nNewLeaderId)
{
    if (!HasMember(nNewLeaderId))
    {
        return;
    }
    if (nNewLeaderId <= 0)
    {
        return;
    }
    m_nLeaderId = nNewLeaderId;
    emc::i().emit<TeamEventES>(emid(), E_CHANGE_TEAM_LEADER, m_nTeamId, nNewLeaderId);
}

membermap_type CreateTeamParam::kMemberV;

TeamTarget CreateTeamParam::kTeamTarget;
