#ifndef TEAM_H
#define TEAM_H

#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>


#include "BaseDef.h"
#include "PublicStruct.pb.h"
#include "GenCode/Team/TeamRpc.pb.h"
#include "CommonLogic/GameEvent/EmidClass.h"
#include "CommonLogic/GameEvent/GameEvent.h"

enum eTeamTargetTypeEnum
{
    E_TEAM_NO_TARGET = -4,
    E_TEAM_SURROUANDING = -3,
    E_TEAM_TARGET_TYPE_ALL = -2,
};

enum eTeamRequestNoTeamTypeEnum
{
    E_SURROUADING_TEAM_MEMBER = 0,
    E_GUILD_TEAM_MEMBER = 1,
    E_FRIEND_TEAM_MEMBER = 2,
};

using  team_id_type = uint64_t ;

typedef std::unordered_map<GUID_t, TeamMemberInfo> membermap_type;

struct CreateTeamParam
{
    static  membermap_type kMemberV;
    static  TeamTarget kTeamTarget;

    team_id_type nTeamId{ 0 };
    GUID_t nLeaderId{ 0 };
    INT nTeamType{ 0 };
    uint64_t emid_{0};
    const membermap_type & v;
    const TeamTarget & target;
    std::string  name_;
};

class Team : public Emid
{
public:

    typedef uint64_t team_id_type;
    typedef std::function<void(GUID_t, uint32_t, ::google::protobuf::Message&)> send_callback_type;
	
	typedef std::unordered_map<GUID_t, TeamMemberInfo> applymap_type;
	typedef std::vector<GUID_t> roleid_list_type;
	
	typedef std::deque<TeamMemberInfo> invitationids_type;
	
    enum eFollowStatus
    {
        E_FREE,
        E_FOLLOW,
    };

	enum EnumTeamType
	{
		E_TEAM_TYPE,
		E_GROUP_TYPE,
        E_REALATION_UION_TYPE,
		E_TEAM_TYPE_MAX
	};

	enum EnumTeamSize
	{
		E_TEAM_SIZE = 5,
		E_GROUP_SIZE = 10,
        E_RELATION_UNION_TYPE = 10,
	};


    enum eTeamEvent
    {
        E_JOIN_TEAM,
        E_BEFORE_LEADER_LEAVE_TEAM,//队长离开队伍
        E_BEFORE_MEMBER_LEAVE_TEAM,//成员离开队伍 
        E_CHANGE_TARGET_TYPE,
        E_APPOINT_LEADER,
        E_CHANGE_TEAM_TYPE,
        E_CREATE_TEAM_JOIN_TYPE,
        E_TEAM_DISMISS_TYPE,
        E_CREATE_TEAM_TYPE,
        E_BEFORE_LEAVE_TEAM_TYPE,//包含E_LEADER_LEAVE_TEAM, E_MEMBER_LEAVE_TEAM
        E_MEMBER_NAME_CHANGE_TTYPE,
		E_CHANGE_TEAM_LEADER,
        E_FOLLOW_MEMBER,
        E_CANCEL_FOLLOW_MEMBER,
        E_MEMBER_CHANGE_FOLLOW_STATE,
    };

    enum eEnumOnLine
    {
        E_OFF_LINE,
        E_ON_LINE,
    };

	static const int32_t MAX_APPLY = 10;
	static const int32_t MAX_INVITATION = 20;
    static const int32_t EmptyTeamId = 0;
    static const TeamTarget kTeamTarget;



    Team(const CreateTeamParam& param);

    virtual ~Team() {}

 

    void Clear()
    {
        m_nTeamId = EmptyTeamId;
        m_nLeaderId = INVALID_ID;
        m_nTeamType = INVALID_ID;
        m_vMembers.clear();
        m_vRoleIdList.clear();

        m_vApplyMembers.clear();
        m_vApplyRoleIdList.clear();

        m_vInvitations.clear();
    }

	void CopyTo(TeamMemberInfo & to)
	{
		
		membermap_type::iterator mit = m_vMembers.find(to.roleid());
		if (mit == m_vMembers.end())
		{
			return;
		}
        
		to.CopyFrom(mit->second);
	}

    

	void CopyTo(TeamInfo & to,bool withrobot = true)
	{
		to.Clear();
		to.set_leaderroleid(m_nLeaderId);
		to.set_teamtype(m_nTeamType);
		to.set_teamid(m_nTeamId);

		for (roleid_list_type::iterator it = m_vRoleIdList.begin(); it != m_vRoleIdList.end(); ++it)
		{
			membermap_type::iterator mit = m_vMembers.find(*it);
			if (!withrobot && mit->second.robot())
			{
				continue;
			}
			to.add_members()->CopyFrom(mit->second);
		}
        to.mutable_teamtarget()->CopyFrom(m_oTarget);
	}

	void CopyFrom(const TeamInfo & from)
	{
		m_vMembers.clear();
		m_vRoleIdList.clear();
		m_nLeaderId = from.leaderroleid();
		m_nTeamId = from.teamid();
		m_nTeamType = from.teamtype();
		for (INT i = 0; i < from.members_size(); ++i)
		{
			m_vMembers.emplace(from.members(i).roleid(), from.members(i));
			m_vRoleIdList.push_back(from.members(i).roleid());
		}
        m_oTarget.CopyFrom(from.teamtarget());
	}

    void CopyFrom(TeamMemberInfo & from);

    TeamMemberInfo & GetMemberInfo(GUID_t roleId)
    {
        membermap_type::iterator mit = m_vMembers.find(roleId);
        if (mit == m_vMembers.end())
        {
            static TeamMemberInfo m;
            return m;
        }

        return mit->second;
    }

    INT JoinTeam(const TeamMemberInfo & m);
    INT TryToJoinTeam(const TeamMemberInfo & m);

	GUID_t ReplaceRobot(TeamMemberInfo &m);

    void OnJoinTeam(const TeamMemberInfo & m);

    INT LeaveTeam(GUID_t nRoleId);
    void ReElectTeamLeader(GUID_t nRoleId);

    void OnLeaveTeam(GUID_t nRoleId);


    void Logout( GUID_t nRoleId);
    void Login(GUID_t nRoleId);

    INT KickMem(GUID_t nCurLeaderId, GUID_t  nKickRoleId);


	INT AppointLeader(GUID_t nCurLeaderId, GUID_t  nNewLeaderRoleId);
	

    void OnAppointLeader(GUID_t  nNewLeaderRoleId);
	

    INT Apply(const TeamMemberInfo & m);
    GUID_t FirstApplyRoleId();

    INT AgreeApplicant(GUID_t nApplyRoleId);

    INT RemoveApply(GUID_t nApplyRoleId);

    INT Invite(TeamMemberInfo & oBeInvite);

	void OnRemoveBeginInvite()
	{
		if (m_vInvitations.empty())
		{
			return;
		}
		m_vInvitations.pop_front();
	}

    INT AgreeInvite(GUID_t nInvite);

	std::size_t GetInviteSize()const
	{
		return m_vInvitations.size();
	}

	bool HasApply(GUID_t nApplyRoleId)
	{
		applymap_type::iterator it = m_vApplyMembers.find(nApplyRoleId);
		return it != m_vApplyMembers.end();
	}

    bool HasApply()
    {
        return !m_vApplyMembers.empty();
    }

	// if m not full , this function will fill it!
	INT UpdateTeamMemInfo(TeamMemberInfo & m);
    INT UpdateTeamMemInfoGs(TeamMemberInfo & m);

	INT GetTeamMemInfo(TeamMemberInfo & m);

    static bool IsFollow(const TeamMemberInfo& m);
    bool IsFollowMemberOrLeader(GUID_t roleId)const;
    bool IsFollow(GUID_t roleId)const;
    bool IsRobot(GUID_t roleId)const;
	bool HasRobot()
	{
		for (auto it : m_vMembers)
		{
			if (it.second.robot())
				return true;
		}
		return false;
	}
	int GetRobotSize()
	{
		int ret = 0;
		for (auto it : m_vMembers)
		{
			if (it.second.robot())
				ret++;
		}
		return ret;
	}

	void ClearRobotLocationData();

    int32_t SetFollowState(GUID_t roleId, int32_t s);
    int32_t GetFollowState(GUID_t roleId);

    bool IsOnline(GUID_t roleId)const
    {
        membermap_type::const_iterator it = m_vMembers.find(roleId);
        if (it == m_vMembers.end())
        {
            return false;
        }
        return it->second.online() == E_ON_LINE;
    }

    std::size_t GetOnlineMemberSize()
    {
        std::size_t nOnlineSize = 0;
        for (membermap_type::iterator it = m_vMembers.begin(); it != m_vMembers.end(); ++it)
        {
            if (it->second.online() == E_ON_LINE)
            {
                ++nOnlineSize;
            }
        }
        return nOnlineSize;
    }

	team_id_type GetTeamId()const
	{
		return m_nTeamId;
	}

	bool IsFull()const
	{
		return m_vMembers.size() >= GetMaxSize();
	}

    bool IsAllMemberOffLine()const;
    

	bool IsSameGuild()
	{
		auto iter = m_vMembers.begin();
		if (iter == m_vMembers.end())
		{
			return false;
		}

		GUID_t guildguid = iter->second.guildguid();
		for (auto iter0 : m_vMembers)
		{
			GUID_t temGuildGuid = iter0.second.guildguid();
			if (temGuildGuid != guildguid)
			{
				return false;
			}
		}

		return true;
	}

	virtual std::size_t GetMaxSize()const
	{
		if (m_nTeamType == E_TEAM_TYPE)
		{
			return  E_TEAM_SIZE;
		}
		else if (m_nTeamType == E_GROUP_TYPE)
		{
			return E_GROUP_SIZE;
		}
		return 0;
	}

    static std::size_t GetMaxSize(int32_t nTeamType)
    {
        if (nTeamType == E_TEAM_TYPE)
        {
            return  E_TEAM_SIZE;
        }
        else if (nTeamType == E_GROUP_TYPE)
        {
            return E_GROUP_SIZE;
        }
        else if (nTeamType == E_REALATION_UION_TYPE)
        {
            return E_GROUP_SIZE;
        }
        return 0;
    }

	INT GetTeamType()const
	{
		return m_nTeamType;
	}

    TeamTarget GetTeamTarget()const
    {
        return m_oTarget;
    }

    bool IsGroup()const
    {
        return GetTeamType() == E_GROUP_TYPE;
    }

    bool IsNormalTeamType()const
    {
        return GetTeamType() == E_TEAM_TYPE;
    }
    
	roleid_list_type   GetTeamRoleIds()const
	{
        roleid_list_type v;
        v.push_back(GetLeaderId());
        for (auto rid : m_vRoleIdList)
        {
            if (rid == GetLeaderId())
            {
                continue;
            }
            v.push_back(rid);
        }
		return v;
	}

    roleid_list_type   GetTeamFollowRoleIds()const
    {
        roleid_list_type v;
        v.push_back(GetLeaderId());
        for (auto rid : m_vRoleIdList)
        {
            if (rid == GetLeaderId())
            {
                continue;
            }
            if (!IsFollow(rid))
            {
                continue;
            }
            v.push_back(rid);
        }
        return v;
    }

	bool InThisTeam(GUID_t nRoleId)
	{
		return m_vMembers.find(nRoleId) != m_vMembers.end() && m_vRoleIdList.end() != std::find(m_vRoleIdList.begin(), m_vRoleIdList.end(), nRoleId);
	}

	std::size_t GetMemSize()
	{
		return m_vMembers.size();
	}

	bool Empty()const
	{
		return m_vMembers.empty();
	}

	GUID_t GetLeaderId()const
	{
		return m_nLeaderId;
	}

    GUID_t GetFirstMemberRoleId();

	//just call for reply
    void SetLeaderId(GUID_t nNewLeaderId);
	
	bool HasTeam()const
	{
		return m_nTeamId != EmptyTeamId;
	}

    bool IsLeader(GUID_t nRoleId)const
    {
        return (int)m_nLeaderId != INVALID_ID && m_nLeaderId == nRoleId;
    }

	std::size_t GetRoleIdSize()const
	{
		return m_vRoleIdList.size();
	}

	std::size_t GetApplySize()const
	{
#ifdef __TEST__
		assert(m_vApplyRoleIdList.size() == m_vApplyMembers.size());
#endif // __TEST__
		return m_vApplyMembers.size();
	}
#ifdef __TEST__
	GUID_t GetApplyBeginRoleId()const
	{
		syassertn(!m_vApplyRoleIdList.empty());
		return *m_vApplyRoleIdList.begin();
	}

	bool TestApplyEq()const
	{
		for (roleid_list_type::const_iterator it = m_vApplyRoleIdList.begin(); it != m_vApplyRoleIdList.end(); ++it)
		{
			if (m_vApplyMembers.find(*it) == m_vApplyMembers.end())
			{
				return false;
			}
		}
		return true;
	}
#endif // __TEST__

    int32_t SetTarget(GUID_t nRoleId, const TeamTarget & target);
    int32_t CopyTarget(const TeamTarget& target);
    int32_t OnChangeName(GUID_t role_id, const std::string& n);
    int32_t CheckLimt(const TeamMemberInfo & m);

    virtual int32_t ChangeTeamType(GUID_t nRoleId, int32_t ntype);

    void ToTeamTarget(TeamTarget & target);

    void GetTeamApplyList(::google::protobuf::RepeatedPtrField< ::TeamMemberInfo >*pv);
 
    void ClearApplyList();

    bool SameTarget(int32_t teamgroup, int32_t teamgroupid);

    void SetTeamType(int32_t teamType);

    int32_t GetMemberMinLevel();
    int32_t GetMemberMaxLevel();
    int32_t GetMemberAverageLevel();
	int32_t GetMemberMinBattleScore();
	int32_t GetMemberAverageBattleScore();
	int32_t GetMemberAverageLevelWithOutRobot();

    const membermap_type & GetMembers()
    {
        return m_vMembers;
    }

    bool HasMemberNotInBaseScene();
    static bool NotInBaseScene(const TeamMemberInfo & m);
    bool NotInBaseScene(GUID_t m);
    bool IsInBaseScene(GUID_t roleId);
    static bool InBaseScene(const TeamMemberInfo & m);
    bool InMirrorScene(const TeamMemberInfo & m);
	bool IsMemberDie();

    void SetSendMsgCallback(send_callback_type & cb) { send_callback_ = cb; }
    void SetSendToSceneMsgCallback(const send_callback_type & cb) { send_to_scene_callback_ = cb; }

    virtual void OnEraseRole(GUID_t player_guid) {}
    virtual void SaveAllMemberToDb() {}

    void OnCreate();

    bool HasMember(GUID_t player_guid);

    int32_t FollowMember(GUID_t player_id, GUID_t be_follow_player_id);
    int32_t CancelFollowMember(GUID_t rolplayer_ide_id);

protected:
    team_id_type m_nTeamId;
    GUID_t m_nLeaderId;
    INT m_nTeamType;

    membermap_type m_vMembers;
    roleid_list_type m_vRoleIdList;

    applymap_type m_vApplyMembers;
	roleid_list_type m_vApplyRoleIdList;

	invitationids_type m_vInvitations;
    TeamTarget m_oTarget;

    send_callback_type send_callback_;
    send_callback_type send_to_scene_callback_;
};



#endif // TEAM_H