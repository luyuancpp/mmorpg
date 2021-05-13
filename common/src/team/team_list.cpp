#include "TeamList.h"

#include "GenCode/GameDefine_Result.h"
#include "CommonLogic/EventStruct/TeamEventStruct.h"


int32_t TeamList::CreateTeam(TeamMemberInfo & oLeader, int32_t nTeamType, Team::team_id_type & nTeamId, const TeamTarget & target)
{

    if (nTeamType < Team::E_TEAM_TYPE || nTeamType > Team::E_GROUP_TYPE)
    {
        return OR_TEAM_TYPE_ERROR;
    }

    if (IsTeamListFull())
    {
        return OR_TEAM_LIST_FULL;
    }

    if (RoleInTeam(oLeader.roleid()))
    {
        return OR_IN_TEAM;
    }
   
    nTeamId = m_oTeamSnowflake.generate(nTeamId);

    membermap_type v;
    v.emplace(oLeader.roleid(), oLeader);
    CreateTeamParam param{ nTeamId, oLeader.roleid(), nTeamType, emid(), v,  target, std::string("") };

    team_ptr p_team(
        new Team(param));
    std::pair<teamlist_type::iterator, bool> p = m_vTeams.emplace(nTeamId, std::move(p_team));
    if (p.second)
    {
        p.first->second->emid(emp_->emid());
        p.first->second->OnCreate();
    }

    return OR_OK;

}

TeamList::TeamList()
    :emp_(emc::i().createmanger())
{
    emid(emp_->emid());
    emp_->subscribe<TeamEventES>(*this);
}

int32_t TeamList::JoinTeam(Team::team_id_type nTeamId, TeamMemberInfo & mem)
{
	if(!mem.robot())
    if (RoleInTeam(mem.roleid()))
    {
        return OR_IN_TEAM;
    }
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }
	int ret = it->second->JoinTeam(mem);

    return ret;
}

void TeamList::EraseTeam(Team::team_id_type nTeamId)
{
    emc::i().emit<TeamEventES>(emid(), Team::E_TEAM_DISMISS_TYPE, nTeamId, 0);
    m_vTeams.erase(nTeamId);
}

GUID_t TeamList::ReplaceRobot(Team::team_id_type nTeamId, TeamMemberInfo & mem)
{
	teamlist_type::iterator it = m_vTeams.find(nTeamId);
	if (it != m_vTeams.end())
	{
		return it->second->ReplaceRobot(mem);
	}
	return 0;
}

int32_t TeamList::LeaveTeam(Team::team_id_type nTeamId, GUID_t nRoleId)
{
    auto p_team = GetTeamPtr(nTeamId);
    if (nullptr == p_team)
    {
        return OR_TEAM_ID_ERROR;
    }

    OR_CHECK_RESULT(p_team->LeaveTeam(nRoleId));
    if (p_team->Empty() || (HasRobot(nTeamId) && IsAllMemberOffLine(nTeamId)))
    {
        EraseTeam(nTeamId);
    }
 
       
    return OR_OK;
}

int32_t TeamList::KickMem(Team::team_id_type nTeamId, GUID_t nCurLeaderId, GUID_t  nKickRoleId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }
    OR_CHECK_RESULT(it->second->KickMem(nCurLeaderId, nKickRoleId));

    return OR_OK;
}

int32_t TeamList::DissMissTeam(Team::team_id_type nTeamId, GUID_t nCurLeaderId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }

    if (it->second->GetLeaderId() != nCurLeaderId)
    {
        return OR_NOT_LEADER;
    }

    EraseTeam(nTeamId);
    return OR_OK;
}

int32_t TeamList::DissMissTeamNoLeader(Team::team_id_type nTeamId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }

    EraseTeam(nTeamId);
    return OR_OK;
}

int32_t TeamList::AppointLeader(Team::team_id_type nTeamId, GUID_t nCurLeaderId, GUID_t  nNewLeaderRoleId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }
    return it->second->AppointLeader(nCurLeaderId, nNewLeaderRoleId);
}

int32_t TeamList::Apply(Team::team_id_type nTeamId, const TeamMemberInfo & m)
{
	if(!m.robot())
    if (RoleInTeam(m.roleid()))
    {
        return OR_IN_TEAM;
    }
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }
    return it->second->Apply(m);
}

GUID_t TeamList::FirstApplyRoleId(Team::team_id_type nTeamId)
{
    
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }
    return it->second->FirstApplyRoleId();
}

int32_t TeamList::RemoveApply(Team::team_id_type nTeamId, GUID_t nApplyRoleId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }
    return it->second->RemoveApply(nApplyRoleId);
}

int32_t TeamList::AgreeApplicant(Team::team_id_type nTeamId, GUID_t nApplyRoleId, bool robot /*= false*/)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }
	if(!robot)
    if (RoleInTeam(nApplyRoleId))
    {
        it->second->RemoveApply(nApplyRoleId);
        return OR_IN_TEAM;
    }

	return it->second->AgreeApplicant(nApplyRoleId);

}

int32_t TeamList::Invite(Team::team_id_type nTeamId, TeamMemberInfo & oBeInvite)
{
    if (RoleInTeam(oBeInvite.roleid()))
    {
        return OR_IN_TEAM;
    }
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }
    return it->second->Invite(oBeInvite);
}

int32_t TeamList::AgreeInvite(Team::team_id_type nTeamId, GUID_t nAgreeInvite)
{

    if (RoleInTeam(nAgreeInvite))
    {
        return OR_IN_TEAM;
    }
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
    }

	return  it->second->AgreeInvite(nAgreeInvite);

}

bool TeamList::IsValidTeamType(int32_t nTeamType)
{
    return nTeamType >= Team::E_TEAM_TYPE && nTeamType < Team::E_TEAM_TYPE_MAX;
}



bool TeamList::IsTeamListFull()const
{
    return m_vTeams.size() >= MAX_TEAM_COUNT;
}

bool TeamList::IsAllMemberOffLine(Team::team_id_type nTeamId) const
{
    teamlist_type::const_iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return true;
    }
    return it->second->IsAllMemberOffLine();
}

bool TeamList::IsTeamFull(Team::team_id_type nTeamId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return true;
    }
    return it->second->IsFull();
}

bool TeamList::HasRobot(Team::team_id_type nTeamId)
{
	teamlist_type::iterator it = m_vTeams.find(nTeamId);
	if (it == m_vTeams.end())
	{
		return false;
	}
	return it->second->HasRobot();
}

std::size_t TeamList::TeamSize()const
{
    return m_vTeams.size();
}


bool TeamList::InTeam(Team::team_id_type nTeamId, GUID_t nRoleId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return false;
    }
    return it->second->InThisTeam(nRoleId);
}



void TeamList::CopyTo(Team::team_id_type nTeamId, TeamInfo & to,bool withrobot)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return;
    }
    it->second->CopyTo(to,withrobot);
}

TeamInfo TeamList::GetTeamInfo(Team::team_id_type nTeamId) 
{ 
    TeamInfo t;  
    CopyTo(nTeamId, t); 
    return t;
}

TeamMemberInfo TeamList::GetTeamMemberInfo(GUID_t nRoleId)
{
    Team::team_id_type teamId = GetTeamId(nRoleId);
    if (teamId == Team::EmptyTeamId)
    {
        return TeamMemberInfo();
    }
    teamlist_type::iterator it = m_vTeams.find(teamId);
    if (it == m_vTeams.end())
    {
        return TeamMemberInfo();
    }
    return it->second->GetMemberInfo(nRoleId);
}

TeamInfo TeamList::GetTeamInfoFromRoleId(GUID_t nRoleId)
{
    Team::team_id_type teamId = GetTeamId(nRoleId);
    return GetTeamInfo(teamId);
}

void TeamList::SetFollowState(GUID_t role_id, int32_t s)
{
    Team::team_id_type teamId = GetTeamId(role_id);
    teamlist_type::iterator it = m_vTeams.find(teamId);
    if (it == m_vTeams.end())
    {
        return;
    }
    it->second->SetFollowState(role_id, s);
}

void TeamList::CopyTo(Team::team_id_type nTeamId, TeamMemberInfo & to)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return;
    }
    it->second->CopyTo(to);
}

void TeamList::CopyFrom(Team::team_id_type nTeamId, TeamMemberInfo & from)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return;
    }
    it->second->CopyFrom(from);
}


std::size_t TeamList::GetMemSize(Team::team_id_type nTeamId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return 0;
    }
    return it->second->GetMemSize();
}


bool TeamList::ValidTeam(Team::team_id_type nTeamId)
{
    return m_vTeams.find(nTeamId) != m_vTeams.end();
}

Team::roleid_list_type  TeamList::GetTeamRoleIds(Team::team_id_type nTeamId)const
{
    Team::roleid_list_type ret;
    const teamlist_type::const_iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return ret;
    }

    return it->second->GetTeamRoleIds();
}

const Team::roleid_list_type  TeamList::GetTeamFollowRoleIds(Team::team_id_type nTeamId)const
{
    Team::roleid_list_type ret;
    const teamlist_type::const_iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return Team::roleid_list_type();
    }

    return it->second->GetTeamFollowRoleIds();
}

bool TeamList::IsFollowLeader(GUID_t role_id)
{
    team_id_type my_team_id = GetTeamId(role_id);
    if (my_team_id == Team::EmptyTeamId)
    {
        return false;
    }
    teamlist_type::iterator it = m_vTeams.find(my_team_id);
    if (it == m_vTeams.end())
    {
        return false;
    }
    if (it->second->IsLeader(role_id))
    {
        return false;
    }
    return it->second->IsFollow(role_id);
}

bool TeamList::IsFollowMemberOrLeader(GUID_t role_id)
{
    team_id_type my_team_id = GetTeamId(role_id);
    if (my_team_id == Team::EmptyTeamId)
    {
        return false;
    }
    teamlist_type::iterator it = m_vTeams.find(my_team_id);
    if (it == m_vTeams.end())
    {
        return false;
    }
    return it->second->IsFollowMemberOrLeader(role_id);
}

void TeamList::ReElectTeamLeader(GUID_t role_id)
{
    team_id_type my_team_id = GetTeamId(role_id);
    if (my_team_id == Team::EmptyTeamId)
    {
        return ;
    }
    auto p_team = GetTeamPtr(my_team_id);
    if (nullptr == p_team)
    {
        return;
    }
    return p_team->ReElectTeamLeader(role_id);
}

int32_t TeamList::GetMemberMinLevel(Team::team_id_type nTeamId)
{
    const teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return 0;
    }

    return it->second->GetMemberMinLevel();
}

int32_t TeamList::GetMemberMaxLevel(Team::team_id_type nTeamId)
{
    const teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return 0;
    }

    return it->second->GetMemberMaxLevel();
}


int32_t TeamList::GetMemberAverageLevel(Team::team_id_type nTeamId)
{
    const teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return 0;
    }

    return it->second->GetMemberAverageLevel();
}

const membermap_type & TeamList::GetMembers(Team::team_id_type nTeamId)
{
    
    const teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        static membermap_type r;
        return r;
    }
    return it->second->GetMembers();
}

GUID_t TeamList::GetLearderRoleId(Team::team_id_type nTeamId)const
{
    const teamlist_type::const_iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return INVALID_ID;
    }
    return it->second->GetLeaderId();
}

GUID_t TeamList::GetFirstMemberRoleId(Team::team_id_type nTeamId) const
{
    const teamlist_type::const_iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return 0;
    }
    return it->second->GetFirstMemberRoleId();
}

bool TeamList::RoleInTeam(GUID_t nRoleId)const
{
    return m_vRoleTeamList.find(nRoleId) != m_vRoleTeamList.end();
}

Team::team_id_type TeamList::GetTeamId(GUID_t nRoleId)
{
    roleteamidlist_type::iterator it = m_vRoleTeamList.find(nRoleId);
    if (it != m_vRoleTeamList.end())
    {
        return it->second;
    }

    return Team::EmptyTeamId;
}

bool TeamList::HasTeam(GUID_t nRoleId)
{
    return GetTeamId(nRoleId) != Team::EmptyTeamId;
}

TeamList::team_ptr TeamList::GetTeamPtr(Team::team_id_type team_id)
{
    teamlist_type::iterator it = m_vTeams.find(team_id);
    if (it == m_vTeams.end())
    {
        return nullptr;
    }
    return it->second;
}

bool TeamList::IsLeader(Team::team_id_type nTeamId, GUID_t nRoleId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it != m_vTeams.end())
    {
        return it->second->IsLeader(nRoleId);
    }
    return false;
}

bool TeamList::IsLeader(GUID_t nRoleId)
{
    roleteamidlist_type::iterator it = m_vRoleTeamList.find(nRoleId);
    if (it == m_vRoleTeamList.end())
    {
        return false;
    }
    return IsLeader(it->second, nRoleId);
}

bool TeamList::IsRobot(GUID_t nRoleId)
{
    roleteamidlist_type::iterator it = m_vRoleTeamList.find(nRoleId);
    if (it == m_vRoleTeamList.end())
    {
        return false;
    }
    teamlist_type::iterator tid = m_vTeams.find(it->second);
    if (tid == m_vTeams.end())
    {
        return false;
    }
    return tid->second->IsRobot(nRoleId);
}


int32_t TeamList::SetTarget(Team::team_id_type nTeamId, GUID_t nRoleId, const TeamTarget & target)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it != m_vTeams.end())
    {
        return it->second->SetTarget(nRoleId, target);
    }

    return OR_TEAM_ID_ERROR;
}


TeamTarget TeamList::GetTeamTarget(Team::team_id_type nTeamId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it != m_vTeams.end())
    {
        return it->second->GetTeamTarget();
    }
    return TeamTarget();
}

int32_t TeamList::OnChangeName(Team::team_id_type role_id, const std::string& n)
{
    auto team_id = GetTeamId(role_id);
    teamlist_type::iterator it = m_vTeams.find(team_id);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_ID_ERROR;
        
    }
    return it->second->OnChangeName(role_id, n);
}

int32_t TeamList::ChangeTeamType(Team::team_id_type nTeamId, GUID_t nRoleId, int32_t ntype)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it != m_vTeams.end())
    {
        return it->second->ChangeTeamType(nRoleId, ntype);
    }

    return OR_TEAM_ID_ERROR;
}

std::size_t TeamList::GetTeamMaxMemberSize(team_id_type nTeamId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return 0;
    }
    return it->second->GetMaxSize();
}

void TeamList::ToTeamTarget(Team::team_id_type nTeamId, TeamTarget & target)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it != m_vTeams.end())
    {
        return it->second->ToTeamTarget(target);
    }

}

void TeamList::GetTeamApplyList(Team::team_id_type nTeamId, ::google::protobuf::RepeatedPtrField< ::TeamMemberInfo > * pv)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it != m_vTeams.end())
    {
         it->second->GetTeamApplyList(pv);
    }
}

void TeamList::ClearApplyList(Team::team_id_type nTeamId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it != m_vTeams.end())
    {
        it->second->ClearApplyList();
    }
}

void TeamList::Logout(Team::team_id_type nTeamId, GUID_t nRoleId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return;
    }
    return it->second->Logout(nRoleId);
}

void TeamList::Login(Team::team_id_type nTeamId, GUID_t nRoleId)
{
    teamlist_type::iterator it = m_vTeams.find(nTeamId);
    if (it == m_vTeams.end())
    {
        return;
    }
    return it->second->Login(nRoleId);
}

void TeamList::GetTargetTeam(int32_t teamgroup, int32_t teamgroupid, ::google::protobuf::RepeatedPtrField< ::TeamInfo >* pPb, GUID_t role_id)
{
    team_id_type my_team_id =  GetTeamId(role_id);
    for (auto & it : m_vTeams)
    {
        if (it.second->IsFull())
        {
            continue;
        }
        if (it.second->GetTeamId() == my_team_id)
        {
            continue;
        }
        //È«²¿¶ÓÎé
        if (teamgroup == E_TEAM_TARGET_TYPE_ALL)
        {
            it.second->CopyTo(*pPb->Add());
        }
        else
        {
            if (it.second->SameTarget(teamgroup, teamgroupid))
            {
                it.second->CopyTo(*pPb->Add());
            }
        }
        
    }
  
}

void TeamList::receive(const TeamEventES& param)
{
    int32_t eventId = param.team_event_type_;
    team_id_type team_id = param.team_id_;
     GUID_t  m = param.player_id_;
    if (eventId == Team::E_JOIN_TEAM || Team::E_CREATE_TEAM_JOIN_TYPE == eventId)
    {
        m_vRoleTeamList.emplace(m, team_id);
    }

    if (eventId == Team::E_BEFORE_LEADER_LEAVE_TEAM || 
        eventId == Team::E_BEFORE_MEMBER_LEAVE_TEAM || 
        eventId == Team::E_BEFORE_LEAVE_TEAM_TYPE)
    {
        m_vRoleTeamList.erase(m);
    }

    if (eventId ==  Team::E_TEAM_DISMISS_TYPE)
    {
        auto p_team = GetTeamPtr(team_id);
        if (nullptr != p_team)
        {
            auto v = p_team->GetTeamRoleIds();
            for (auto& it : v)
            {
                m_vRoleTeamList.erase(it);
            }
        }
    }
}

int32_t TeamList::CheckLimt(GUID_t nTeamRoleId, const TeamMemberInfo & m)
{
    Team::team_id_type teamId = GetTeamId(nTeamRoleId);
    teamlist_type::iterator it = m_vTeams.find(teamId);
    if (it == m_vTeams.end())
    {
        return OR_TEAM_LEVEL_LIMIT_ERROR;
    }
    return it->second->CheckLimt(m);
}


int32_t TeamList::FollowMember(GUID_t player_id, GUID_t be_follow_player_id)
{
    Team::team_id_type team_id = GetTeamId(player_id);
    if (team_id == Team::EmptyTeamId)
    {
        return OR_CREATE_TEAM_ID_ERORR;
    }
    auto tit = m_vTeams.find(team_id);
    if (tit == m_vTeams.end())
    {
        return OR_CREATE_TEAM_ID_ERORR;
    }
    return tit->second->FollowMember(player_id, be_follow_player_id);
}

int32_t TeamList::CancelFollowMember(GUID_t player_id)
{
    Team::team_id_type team_id = GetTeamId(player_id);
    if (team_id == Team::EmptyTeamId)
    {
        return OR_CREATE_TEAM_ID_ERORR;
    }
    auto tit = m_vTeams.find(team_id);
    if (tit == m_vTeams.end())
    {
        return OR_CREATE_TEAM_ID_ERORR;
    }
    return tit->second->CancelFollowMember(player_id);
}

emc::manager_sp& TeamList::emp()
{
    return emp_;
}

void TeamList::ClearnNotRelationInfo(TeamMemberInfo & m)
{
    m.clear_pinchdata();
    m.clear_treasureinfo();
    m.clear_buffidlist();
    m.clear_location();
}

int32_t TeamList::TryToJoinTheTeam(const membermap_type & member_list, Team::team_id_type  nTeamId)
{
    auto tit = m_vTeams.find(nTeamId);
    if (tit == m_vTeams.end())
    {
        return OR_CREATE_TEAM_ID_ERORR;
    }
    for (auto& it : member_list)
    {
        int32_t nRet = tit->second->TryToJoinTeam(it.second);
        if (nRet != OR_OK)
        {
            return nRet;
        }
    }
   
    return OR_OK;
}

int32_t TeamList::JoinTeam(const membermap_type & member_list,
    Team::team_id_type  nTeamId)
{
    auto tit = m_vTeams.find(nTeamId);
    if (tit == m_vTeams.end())
    {
        return OR_CREATE_TEAM_ID_ERORR;
    }

    for (auto& it : member_list)
    {
        int32_t nRet = tit->second->JoinTeam(it.second);
        if (nRet != OR_OK)
        {
            return nRet;
        }
    }

    return OR_OK;
}

void TeamList::RelaseTeam(Team::team_id_type nTeamId)
{
    auto p_team = GetTeamPtr(nTeamId);
    if (nullptr == p_team)
    {
        return;
    }
    const auto& rv = p_team->GetTeamFollowRoleIds();
    for (auto& it : rv)
    {
        p_team->LeaveTeam(it);
    }

    EraseTeam(nTeamId);
}
