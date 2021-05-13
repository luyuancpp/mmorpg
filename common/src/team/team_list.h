#ifndef TEAM_LIST_H
#define  TEAM_LIST_H

#include <unordered_map>

#include "Team.h"
#include "CommonLogic/GameEvent/EmidClass.h"
#include "CommonLogic/GameEvent/GameEvent.h"

class TeamInterface
{
public:
	virtual void LeaveTeam(Team::team_id_type nTeamId, GUID_t nRoleId) {}
	virtual void JoinTeam(Team::team_id_type nTeamId, TeamMemberInfo & mem) {}
};

enum eTeamObserverModule
{
    E_WORLD_TEAM_SERVER_MODULE,
    E_WORLD_MATCH_MAKING_SERVER_MODULE,
    E_WORLD_REALTION_UNION_MATCH_MODULE,
};

struct TeamEventES;

class TeamList: public Emid,  public entityx::Receiver<TeamList>
{
public:
    typedef std::shared_ptr<Team> team_ptr;
	typedef std::unordered_map<Team::team_id_type, team_ptr> teamlist_type;
	typedef std::unordered_map<GUID_t, Team::team_id_type> roleteamidlist_type;
    typedef typename Team::team_id_type  team_id_type;
    typedef std::unordered_set<team_id_type> team_id_list_type;
	static const int32_t MAX_TEAM_COUNT = 10000;
    
	
    int32_t CreateTeam(TeamMemberInfo & oLeader, int32_t nTeamType, Team::team_id_type & nTeamId, const TeamTarget & target = Team::kTeamTarget);


    TeamList();
    virtual ~TeamList() {}

    int32_t JoinTeam(Team::team_id_type nTeamId, TeamMemberInfo & mem);
	GUID_t ReplaceRobot(Team::team_id_type nTeamId, TeamMemberInfo & mem);
    int32_t LeaveTeam(Team::team_id_type nTeamId, GUID_t nRoleId);

    int32_t KickMem(Team::team_id_type nTeamId, GUID_t nCurLeaderId, GUID_t  nKickRoleId);

    int32_t DissMissTeam(Team::team_id_type nTeamId, GUID_t nCurLeaderId);
    int32_t DissMissTeamNoLeader(Team::team_id_type nTeamId);

    int32_t AppointLeader(Team::team_id_type nTeamId, GUID_t nCurLeaderId, GUID_t  nNewLeaderRoleId);

    int32_t Apply(Team::team_id_type nTeamId, const TeamMemberInfo & m);
    GUID_t FirstApplyRoleId(Team::team_id_type nTeamId);

    int32_t RemoveApply(Team::team_id_type nTeamId, GUID_t nApplyRoleId);

    int32_t AgreeApplicant(Team::team_id_type nTeamId, GUID_t nApplyRoleId, bool robot = false);

    int32_t Invite(Team::team_id_type nTeamId, TeamMemberInfo & oBeInvite);

    int32_t AgreeInvite(Team::team_id_type nTeamId, GUID_t nAgreeInvite);

    static bool IsValidTeamType(int32_t nTeamType);	

    virtual bool IsTeamListFull()const;
    bool IsAllMemberOffLine(Team::team_id_type nTeamId)const;
    bool IsTeamFull(Team::team_id_type nTeamId);

	bool HasRobot(Team::team_id_type nTeamId);

    std::size_t TeamSize()const;

    Team::team_id_type GetSequence()const;

    bool InTeam(Team::team_id_type nTeamId, GUID_t nRoleId);

    void CopyTo(Team::team_id_type nTeamId, TeamInfo & to,bool withrobot = true);
    TeamInfo GetTeamInfo(Team::team_id_type nTeamId);
    TeamMemberInfo GetTeamMemberInfo(GUID_t nRoleId);
    TeamInfo GetTeamInfoFromRoleId(GUID_t nRoleId);
    void SetFollowState(GUID_t roleId, int32_t s);

    void CopyTo(Team::team_id_type nTeamId, TeamMemberInfo & to);

    void CopyFrom(Team::team_id_type nTeamId, TeamMemberInfo & from);


    std::size_t GetMemSize(Team::team_id_type nTeamId);

    bool ValidTeam(Team::team_id_type nTeamId);

    Team::roleid_list_type  GetTeamRoleIds(Team::team_id_type nTeamId)const;
    const Team::roleid_list_type  GetTeamFollowRoleIds(Team::team_id_type nTeamId)const;
    bool IsFollowLeader(GUID_t role_id);
    bool IsFollowMemberOrLeader(GUID_t role_id);
    void ReElectTeamLeader(GUID_t role_id);
    
    int32_t GetMemberMinLevel(Team::team_id_type nTeamId);
    int32_t GetMemberMaxLevel(Team::team_id_type nTeamId);
    int32_t GetMemberAverageLevel(Team::team_id_type nTeamId);
    const membermap_type & GetMembers(Team::team_id_type nTeamId);
    GUID_t GetLearderRoleId(Team::team_id_type nTeamId)const;
    GUID_t GetFirstMemberRoleId(Team::team_id_type nTeamId)const;

    bool RoleInTeam(GUID_t nRoleId)const;

    Team::team_id_type GetTeamId(GUID_t nRoleId);
    bool HasTeam(GUID_t nRoleId);
    team_ptr GetTeamPtr(Team::team_id_type team_id);

    bool IsLeader(Team::team_id_type nTeamId, GUID_t nRoleId);
    bool IsLeader(GUID_t nRoleId);
    bool IsRobot(GUID_t nRoleId);
    

    int32_t SetTarget(Team::team_id_type nTeamId, GUID_t nRoleId, const TeamTarget & target);
    TeamTarget GetTeamTarget(Team::team_id_type nTeamId);
    int32_t OnChangeName(GUID_t role_id, const std::string& n);
    int32_t ChangeTeamType(Team::team_id_type nTeamId, GUID_t nRoleId, int32_t ntype);

    std::size_t GetTeamMaxMemberSize(team_id_type id);

    void ToTeamTarget(Team::team_id_type nTeamId, TeamTarget & target);


    void GetTeamApplyList(Team::team_id_type nTeamId, ::google::protobuf::RepeatedPtrField< ::TeamMemberInfo >*pv);
    void ClearApplyList(Team::team_id_type nTeamId);

    void Logout(Team::team_id_type nTeamId, GUID_t nRoleId);
    void Login(Team::team_id_type nTeamId, GUID_t nRoleId);
#ifdef __TEST__
	Team * GetTeam(Team::team_id_type nTeamId)
	{
		teamlist_type::iterator it = m_vTeams.find(nTeamId);
		if (it == m_vTeams.end())
		{
			return NULL;
		}
		return it->second.get();
	}
#endif // __TEST__


    void GetTargetTeam(int32_t teamgroup, int32_t teamgroupid, ::google::protobuf::RepeatedPtrField< ::TeamInfo >* pPb, GUID_t role_id);

    void UnRegisterEventCallBack(int32_t nModuleId );
    void receive(const TeamEventES& param);

    int32_t CheckLimt(GUID_t nTeamRoleId, const TeamMemberInfo & m);

    Snowflake & GetSnowflake() { return m_oTeamSnowflake;  }
    void SetInterface(TeamInterface* pFace) { m_pInterface = pFace; }
    TeamInterface* GetInterface() { return m_pInterface; }

    int32_t FollowMember(GUID_t player_id, GUID_t be_follow_player_id);
    int32_t CancelFollowMember(GUID_t rolplayer_id);

    emc::manager_sp& emp();

    static void ClearnNotRelationInfo(TeamMemberInfo & m);


protected:
    
    int32_t TryToJoinTheTeam(const membermap_type & member_list,
        Team::team_id_type  nTeamId);

    int32_t JoinTeam(const membermap_type & member_list,
        Team::team_id_type  nTeamId);

    void EraseTeam(Team::team_id_type nTeamId);
	

    void RelaseTeam(Team::team_id_type nTeamId);

   TeamInterface* m_pInterface = 0;
       
protected:
	teamlist_type m_vTeams;

	roleteamidlist_type m_vRoleTeamList;
    Snowflake m_oTeamSnowflake;
    emc::manager_sp emp_;
};


#endif // TEAM_LIST_H

