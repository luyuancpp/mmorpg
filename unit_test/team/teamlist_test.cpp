#include <gtest/gtest.h>

#include "src/team/team_list.h"
#include "src/return_code/return_notice_code.h"

using namespace common;

TEST(TeamManger, CreateFullDismiss)
{
	TeamList team_list;

	typedef std::vector<GameGuid> PlayerIdsV;
	PlayerIdsV teamidlist;
	TeamMember m;
	GameGuid player_id = 1;
	m.set_player_id(player_id++);
	CreateTeamParam p{ m.player_id(), Members{{m.player_id(), m}}, "" };
	for (int32_t i = 0; i < TeamList::kMaxTeamSize; ++i)
	{	
		m.set_player_id(player_id++);
		EXPECT_EQ(ReturnValue(RET_OK), team_list.CreateTeam(p));
		//EXPECT_TRUE(team_list.last_team_id() > 0);
		teamidlist.push_back(team_list.last_team_id());
	}

	EXPECT_TRUE(team_list.IsTeamsMax());
	m.set_player_id(player_id++);
	EXPECT_EQ(RET_TEAM_TEAM_LIST_MAX, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));

	EXPECT_EQ(TeamList::kMaxTeamSize, team_list.team_size());

	player_id = 1;
	for (auto it = teamidlist.begin(); it != teamidlist.end(); ++it)
	{
		EXPECT_EQ(RET_OK, team_list.DissMissTeam(*it, player_id++));
	}
	EXPECT_EQ(0, team_list.team_size());
}

TEST(TeamManger, TeamSizeTest)
{
	TeamList  team_list;	
	TeamMember m;
	m.set_player_id(100);
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));
	EXPECT_TRUE(team_list.PlayerInTeam(team_list.last_team_id(), m.player_id()));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(1, team_list.member_size(team_list.last_team_id()));

	for (std::size_t i = 1; i < kMaxMemberSize; ++i)
	{
		m.set_player_id(m.player_id() + i);
		EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
		EXPECT_EQ(1 + i, team_list.member_size(team_list.last_team_id()));
	}
	m.set_player_id(m.player_id() + 1);
	EXPECT_EQ(RET_TEAM_MEMBERS_FULL, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(kMaxMemberSize, team_list.member_size(team_list.last_team_id()));
}

TEST(TeamManger, LeaveTeam)
{
	TeamList  team_list;
	TeamMember m;
	m.set_player_id(100);
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));
	EXPECT_TRUE(team_list.PlayerInTeam(team_list.last_team_id(), m.player_id()));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(1, team_list.member_size(team_list.last_team_id()));

	team_list.LeaveTeam(m.player_id() );
    EXPECT_FALSE(team_list.PlayerInTeam(team_list.last_team_id(), m.player_id()));
	EXPECT_EQ(0, team_list.member_size(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_HAS_NOT_TEAM_ID, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(0, team_list.member_size(team_list.last_team_id()));

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));

	GameGuid player_id = m.player_id();
	for (std::size_t i = 1; i < kMaxMemberSize; ++i)
	{
		m.set_player_id(player_id + i);
		EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
		EXPECT_EQ(1 + i, team_list.member_size(team_list.last_team_id()));
	}

	for (std::size_t i = 0; i < kMaxMemberSize; ++i)
	{
		m.set_player_id(player_id + i);
		team_list.LeaveTeam(m.player_id());
        EXPECT_FALSE(team_list.PlayerInTeam(team_list.last_team_id(), m.player_id()));
		if (i < 4)
		{
			EXPECT_EQ(player_id + i + 1, team_list.leader_id_by_teamid(team_list.last_team_id()));
			EXPECT_EQ(kMaxMemberSize - i - 1, team_list.member_size(team_list.last_team_id()));
		}
		
		EXPECT_EQ(kMaxMemberSize - i - 1 , team_list.member_size(team_list.last_team_id()));
	}
}


TEST(TeamManger, KickTeaamMember)
{
	TeamList team_list ;
	TeamMember m;
	m.set_player_id(100);

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));

	EXPECT_EQ(RET_TEAM_KICK_SELF, team_list.KickMember(team_list.last_team_id(), m.player_id(), m.player_id()));
	EXPECT_EQ(RET_TEAM_KICK_NOT_LEADER, team_list.KickMember(team_list.last_team_id(),99, 99));

	m.set_player_id(m.player_id() + 1);
	EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(RET_TEAM_KICK_SELF, team_list.KickMember(team_list.last_team_id(),101, m.player_id()));
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_KICK_NOT_LEADER, team_list.KickMember(team_list.last_team_id(),101, 101));
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_KICK_SELF, team_list.KickMember(team_list.last_team_id(), m.player_id(), m.player_id()));
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_MEMBER_NOT_IN_TEAM, team_list.KickMember(team_list.last_team_id(),100, 88));
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_OK, team_list.KickMember(team_list.last_team_id(),100, 101));
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));
}


TEST(TeamManger, AppointLaderAndLeaveTeam1)
{

	TeamList team_list;
	TeamMember m;
	m.set_player_id(100);

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));

	GameGuid player_id = m.player_id();
	for (std::size_t i = 1; i < kMaxMemberSize; ++i)
	{
		m.set_player_id(player_id + i);
		EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
		EXPECT_EQ(1 + i, team_list.member_size(team_list.last_team_id()));
	}

	EXPECT_EQ(RET_TEAM_APPOINT_SELF, team_list.AppointLeader(team_list.last_team_id(), 101, 101));
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_APPOINT_SELF, team_list.AppointLeader(team_list.last_team_id(), 101, 100));
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_APPOINT_SELF, team_list.AppointLeader(team_list.last_team_id(), 100, 100));
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));

	EXPECT_EQ(RET_OK, team_list.AppointLeader(team_list.last_team_id(), 100, 101));
	EXPECT_EQ(101, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(101);
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));

	EXPECT_EQ(RET_OK, team_list.AppointLeader(team_list.last_team_id(), 100, 102));
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(102);
	EXPECT_EQ(m.player_id(), team_list.leader_id_by_teamid(team_list.last_team_id()));


	EXPECT_EQ(RET_OK, team_list.AppointLeader(team_list.last_team_id(), 100, 103));
	EXPECT_EQ(103, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(103);
	EXPECT_EQ(100, team_list.leader_id_by_teamid(team_list.last_team_id()));

	EXPECT_EQ(RET_OK, team_list.AppointLeader(team_list.last_team_id(), 100, 104));
	EXPECT_EQ(104, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(104);
	EXPECT_EQ(100, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(100);
	EXPECT_FALSE(team_list.FindTeamId(team_list.last_team_id()));
}

TEST(TeamManger, AppointLaderAndLeaveTeam2)
{
	TeamList  team_list;
	TeamMember m;
	m.set_player_id(100);

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));

	m.set_player_id(104);
	EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));

	EXPECT_EQ(RET_OK, team_list.AppointLeader(team_list.last_team_id(), 100, 104));
	EXPECT_EQ(104, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(100);
	EXPECT_EQ(104, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(104 );
	EXPECT_FALSE(team_list.FindTeamId(team_list.last_team_id()));
}


TEST(TeamManger, DismissTeam)
{
	TeamList  team_list;
	TeamMember m;
	m.set_player_id(100);
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));

	m.set_player_id(104);
	EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));

	EXPECT_EQ(RET_TEAM_HAS_NOT_TEAM_ID, team_list.DissMissTeam(0, 104));
	EXPECT_EQ(RET_TEAM_DISMISS_NOT_LEADER, team_list.DissMissTeam(team_list.last_team_id(), 104));
	EXPECT_EQ(RET_OK, team_list.DissMissTeam(team_list.last_team_id(), 100));
	EXPECT_FALSE(team_list.FindTeamId(team_list.last_team_id()));
}

TEST(TeamManger, ApplyFull)
{
	TeamList  team_list;
	TeamMember m;
	m.set_player_id(1001);

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));

	TeamMember a;

	GameGuid nMax = kMaxMemberSize;
	for (GameGuid i = 0; i < nMax; ++i)
	{
		a.set_player_id(i);
		EXPECT_EQ(RET_OK, team_list.Apply(team_list.last_team_id(),  a));
		if (i < 9)
		{
			EXPECT_TRUE(team_list.applicant_size(team_list.last_team_id()) < 10);
		}
		else
		{
			EXPECT_EQ(10, team_list.applicant_size(team_list.last_team_id()));
			EXPECT_EQ(i - 9, team_list.first_applicant_id(team_list.last_team_id()));
		}
	}

	for (GameGuid i = 0; i < nMax - 10; ++i)
	{
		EXPECT_FALSE(team_list.HasApplicant(team_list.last_team_id(), i));
	}

	for (GameGuid i = nMax - 10; i < nMax; ++i)
	{
		EXPECT_TRUE(team_list.HasApplicant(team_list.last_team_id(), i));
	}
}

TEST(TeamManger, ApplicantOrder)
{
    TeamList  team_list;
    TeamMember m;
    m.set_player_id(1001);
    EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));
    TeamMember a;

	GameGuid nMax = kMaxMemberSize;
    for (GameGuid i = 0; i < nMax; ++i)
    {
        a.set_player_id(i);
        EXPECT_EQ(RET_OK, team_list.Apply(team_list.last_team_id(), a));
    }
    EXPECT_EQ(nMax - 10, team_list.first_applicant_id(team_list.last_team_id()));

    for (GameGuid i = 0; i < nMax; ++i)
    {
        a.set_player_id(i);
        EXPECT_EQ(RET_OK, team_list.Apply(team_list.last_team_id(), a));
    }

    EXPECT_EQ(nMax - 10, team_list.first_applicant_id(team_list.last_team_id()));
}



TEST(TeamManger, AgreeApplicant1)
{
	TeamList  team_list;
	TeamMember m;
	m.set_player_id(1001);

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));

	TeamMember a;

	GameGuid nMax = 11;
    for (GameGuid i = 1; i < nMax; ++i)
    {
        a.set_player_id(i);
        EXPECT_EQ(RET_OK, team_list.Apply(team_list.last_team_id(), a));
    }
	for (GameGuid i = 1; i < nMax; ++i)
	{
		a.set_player_id(i);
		
		if (i < kMaxMemberSize)
		{
			EXPECT_EQ(RET_OK, team_list.AgreeApplicant(team_list.last_team_id(), i));
			EXPECT_FALSE(team_list.HasApplicant(team_list.last_team_id(), i));
		}
		else
		{
			EXPECT_EQ(RET_TEAM_MEMBERS_FULL, team_list.AgreeApplicant(team_list.last_team_id(), i));
			EXPECT_TRUE(team_list.HasApplicant(team_list.last_team_id(), i));
		}
	}
    a.set_player_id(6666);
    EXPECT_EQ(RET_TEAM_MEMBERS_FULL, team_list.Apply(team_list.last_team_id(), a));

    EXPECT_EQ(RET_OK, team_list.LeaveTeam(2));


    m.set_player_id(2);
    EXPECT_EQ(RET_OK, team_list.Apply(team_list.last_team_id(), m));
    EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));
    EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.AgreeApplicant(team_list.last_team_id(), 2));
    EXPECT_FALSE(team_list.HasApplicant(team_list.last_team_id(), 2));
}


TEST(TeamManger, RemoveApplicant)
{
	TeamList  team_list;
	TeamMember m;
	m.set_player_id(1001);

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));
	TeamMember a;

	GameGuid nMax = kMaxMemberSize;

	GameGuid player_id = 1;
	for (GameGuid i = 0; i < nMax; ++i)
	{
		a.set_player_id(player_id++);
		EXPECT_EQ(RET_OK, team_list.Apply(team_list.last_team_id(), a));
		if (i % 2 == 0)
		{
			EXPECT_EQ(RET_OK, team_list.RemoveApplicant(team_list.last_team_id(), a.player_id()));
		}
		
		if (i >= 19 && i % 2 != 0)
		{
			EXPECT_EQ(10, team_list.applicant_size(team_list.last_team_id()));
			EXPECT_EQ(i - 17, team_list.first_applicant_id(team_list.last_team_id()));
		}
	}

	EXPECT_TRUE(team_list.TestApplicantValueEqual(team_list.last_team_id()));
}

TEST(TeamManger, AgreeApplicant)
{
	TeamList  team_list;
	TeamMember m;
	m.set_player_id(1001);
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));

	TeamMember a;

	int32_t nMax = kMaxMemberSize;

	GameGuid player_id = 1;
    
    for (int32_t i = 0; i < nMax; ++i)
    {
        a.set_player_id(player_id++);
        EXPECT_EQ(RET_OK, team_list.Apply(team_list.last_team_id(), a));
        if (i > 18)
        {
            EXPECT_EQ(10, team_list.applicant_size(team_list.last_team_id()));
            EXPECT_EQ(i - 8, team_list.first_applicant_id(team_list.last_team_id()));
        }
    }
    player_id = 1;
    GameGuid nBegiplayer_id = 1;
	for (int32_t i = 0; i < nMax; ++i)
	{
		a.set_player_id(player_id++);
		
		if (i > nMax - 10)
		{
			EXPECT_EQ(RET_OK, team_list.AgreeApplicant(team_list.last_team_id(), a.player_id()));
			EXPECT_TRUE(team_list.PlayerInTeam(team_list.last_team_id(), a.player_id()));
            if (nBegiplayer_id == 1)
            {
                nBegiplayer_id = a.player_id();
            }
            
		}
		else
		{
			EXPECT_FALSE(team_list.PlayerInTeam(team_list.last_team_id(), a.player_id()));
		}
	}

	for (uint64_t i = nBegiplayer_id; i < nBegiplayer_id + 9; ++i )
	{
		EXPECT_TRUE(team_list.PlayerInTeam(team_list.last_team_id(), i));
	}

	EXPECT_TRUE(team_list.TestApplicantValueEqual(team_list.last_team_id()));
}

TEST(TeamManger, PlayerTeamId)
{

	TeamList  team_list;
	TeamMember m;
	m.set_player_id(1);

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));
	EXPECT_TRUE(team_list.PlayerInTeam(m.player_id()));
	EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(m.player_id()));

	m.set_player_id(2);
	EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_TRUE(team_list.PlayerInTeam(m.player_id()));
	EXPECT_TRUE(team_list.PlayerInTeam(team_list.last_team_id(), m.player_id()));
	EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(m.player_id()));

	m.set_player_id(3);
	EXPECT_EQ(RET_OK, team_list.Apply(team_list.last_team_id(), m));
	EXPECT_FALSE(team_list.PlayerInTeam(m.player_id()));
	EXPECT_EQ(kEmptyGameGuid, team_list.GetTeamId(m.player_id()));

	EXPECT_EQ(RET_OK, team_list.RemoveApplicant(team_list.last_team_id(), m.player_id()));
	EXPECT_FALSE(team_list.PlayerInTeam(m.player_id()));
	EXPECT_EQ(kEmptyGameGuid, team_list.GetTeamId(m.player_id()));


	EXPECT_EQ(RET_OK, team_list.Apply(team_list.last_team_id(),  m));
	EXPECT_EQ(RET_OK, team_list.AgreeApplicant(team_list.last_team_id(), m.player_id()));
	EXPECT_TRUE(team_list.PlayerInTeam(m.player_id()));
	EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(m.player_id()));


	for (std::size_t i = 4; i < 11; ++i)
	{
		m.set_player_id(i);
		EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
		EXPECT_TRUE(team_list.PlayerInTeam(m.player_id()));
		EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(m.player_id()));
	}

	m.set_player_id(1);
	EXPECT_EQ(RET_OK, team_list.LeaveTeam(m.player_id()));
	EXPECT_FALSE(team_list.PlayerInTeam(m.player_id()));
	EXPECT_EQ(kEmptyGameGuid, team_list.GetTeamId(m.player_id()));
	
	EXPECT_EQ(2, team_list.leader_id_by_teamid(team_list.last_team_id()));

	m.set_player_id(3);
	EXPECT_EQ(RET_OK, team_list.KickMember(team_list.last_team_id(), 2, m.player_id()));
	EXPECT_FALSE(team_list.PlayerInTeam(m.player_id()));
	EXPECT_EQ(kEmptyGameGuid, team_list.GetTeamId(m.player_id()));
	

	EXPECT_EQ(RET_OK, team_list.DissMissTeam(team_list.last_team_id(), 2));
	for (std::size_t i = 4; i < 11; ++i)
	{
		EXPECT_FALSE(team_list.PlayerInTeam(m.player_id()));
		EXPECT_EQ(kEmptyGameGuid, team_list.GetTeamId(m.player_id()));
	}


    EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));

    for (std::size_t i = 4; i < kMaxMemberSize; ++i)
    {
        m.set_player_id(i);
        EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
        EXPECT_TRUE(team_list.PlayerInTeam(m.player_id()));
        EXPECT_EQ(RET_OK, team_list.LeaveTeam(m.player_id()));
        EXPECT_FALSE(team_list.PlayerInTeam(m.player_id()));
    }
}

TEST(TeamManger, PlayerInTeam)
{


	TeamList  team_list;
	TeamMember m;
	m.set_player_id(1);
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_list.last_team_id(), m));

	m.set_player_id(2);
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.Apply(team_list.last_team_id(), m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.AgreeApplicant(team_list.last_team_id(), m.player_id()));


	EXPECT_EQ(RET_TEAM_PLAEYR_ID, team_list.LeaveTeam(m.player_id()));
	EXPECT_EQ(RET_OK, team_list.LeaveTeam(m.player_id()));
	EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));

	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.Apply(team_list.last_team_id(), m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.AgreeApplicant(team_list.last_team_id(), m.player_id()));


	EXPECT_EQ(RET_OK, team_list.LeaveTeam( m.player_id()));
	EXPECT_EQ(RET_OK, team_list.Apply(team_list.last_team_id(), m));
	EXPECT_EQ(RET_OK, team_list.AgreeApplicant(team_list.last_team_id(), m.player_id()));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_list.last_team_id(), m));


	EXPECT_EQ(RET_OK, team_list.LeaveTeam(m.player_id()));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.Apply(team_list.last_team_id(), m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.AgreeApplicant(team_list.last_team_id(), m.player_id()));
}


TEST(TeamManger, AppointLeaderNotInTeam)
{
    TeamList  team_list;
    TeamMember m;
    m.set_player_id(1);
    GameGuid leaderroleid = 1;
    EXPECT_EQ(RET_OK, team_list.CreateTeam({ m.player_id(), Members{{m.player_id(), m}}, "" }));
    for (GameGuid i = leaderroleid + 1; i < 10; i++)
    {
        EXPECT_EQ(RET_TEAM_HAS_NOT_TEAM_ID, team_list.AppointLeader(team_list.last_team_id(), leaderroleid, i));
    }
}


int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);

    /*while (true)
    {
        RUN_ALL_TESTS();
    }*/
	return RUN_ALL_TESTS();
}
