#include <gtest/gtest.h>

#include "src/game_logic/teams/teams.h"
#include "src/return_code/return_notice_code.h"

using namespace common;

TEST(TeamManger, CreateFullDismiss)
{
	Teams team_list;

	typedef std::vector<GameGuid> PlayerIdsV;
	PlayerIdsV teamidlist;
	GameGuid player_id = 1;	
	for (int32_t i = 0; i < Teams::kMaxTeamSize; ++i)
	{	
		CreateTeamParam p{ player_id, UI64USet{player_id}};
		EXPECT_EQ(RET_OK, team_list.CreateTeam(p));
		teamidlist.push_back(team_list.last_team_id());
		++player_id;
	}

	EXPECT_TRUE(team_list.IsTeamsMax());
	player_id++;
	EXPECT_EQ(RET_TEAM_TEAM_LIST_MAX, team_list.CreateTeam({ player_id, UI64USet{player_id}}));

	EXPECT_EQ(Teams::kMaxTeamSize, team_list.team_size());

	for (auto it = teamidlist.begin(); it != teamidlist.end(); ++it)
	{
		auto leader_player_id = team_list.leader_id_by_teamid(*it);
		EXPECT_EQ(RET_OK, team_list.DissMissTeam(*it, leader_player_id));
	}
	EXPECT_EQ(0, team_list.team_size());
	EXPECT_EQ(0, team_list.players_size());
}

TEST(TeamManger, TeamSizeTest)
{
	Teams  team_list;	
	GameGuid m = 100;
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));
	EXPECT_TRUE(team_list.PlayerInTheTeam(team_list.last_team_id(), m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(1, team_list.member_size(team_list.last_team_id()));

	for (std::size_t i = 1; i < kMaxMemberSize; ++i)
	{
		m = m + i;
		EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
		EXPECT_EQ(1 + i, team_list.member_size(team_list.last_team_id()));
	}
	++m;
	EXPECT_EQ(RET_TEAM_MEMBERS_FULL, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(kMaxMemberSize, team_list.member_size(team_list.last_team_id()));
}

TEST(TeamManger, LeaveTeam)
{
	Teams  team_list;
	GameGuid m = 100;
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));
	EXPECT_TRUE(team_list.PlayerInTheTeam(team_list.last_team_id(), m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(1, team_list.member_size(team_list.last_team_id()));

	team_list.LeaveTeam(m );
    EXPECT_FALSE(team_list.PlayerInTheTeam(team_list.last_team_id(), m));
	EXPECT_EQ(0, team_list.member_size(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_HAS_NOT_TEAM_ID, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(0, team_list.member_size(team_list.last_team_id()));

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));

	GameGuid player_id = m;
	for (std::size_t i = 1; i < kMaxMemberSize; ++i)
	{
		player_id = player_id + i;
		EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), player_id));
		EXPECT_EQ(1 + i, team_list.member_size(team_list.last_team_id()));
	}

	player_id = m;
	for (std::size_t i = 0; i < kMaxMemberSize; ++i)
	{
		player_id = player_id + i;
		team_list.LeaveTeam(player_id);
        EXPECT_FALSE(team_list.PlayerInTheTeam(team_list.last_team_id(), player_id));
		if (i < 4)
		{
			EXPECT_EQ(player_id + i + 1, team_list.leader_id_by_teamid(team_list.last_team_id()));
			EXPECT_EQ(kMaxMemberSize - i - 1, team_list.member_size(team_list.last_team_id()));
		}
		
		EXPECT_EQ(kMaxMemberSize - i - 1 , team_list.member_size(team_list.last_team_id()));
	}
    EXPECT_EQ(0, team_list.team_size());
    EXPECT_EQ(0, team_list.players_size());
}


TEST(TeamManger, KickTeaamMember)
{
	Teams team_list ;
	GameGuid m = 100;
	GameGuid leader_plaeyr_id = 100;

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));

	EXPECT_EQ(RET_TEAM_KICK_SELF, team_list.KickMember(team_list.last_team_id(), m, m));
	EXPECT_EQ(RET_TEAM_KICK_NOT_LEADER, team_list.KickMember(team_list.last_team_id(),99, 99));

	m = (m + 1);
	EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_EQ(RET_TEAM_KICK_SELF, team_list.KickMember(team_list.last_team_id(), leader_plaeyr_id, leader_plaeyr_id));
	EXPECT_EQ(leader_plaeyr_id, team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_KICK_NOT_LEADER, team_list.KickMember(team_list.last_team_id(), m, leader_plaeyr_id));
	EXPECT_EQ(leader_plaeyr_id, team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_KICK_NOT_LEADER, team_list.KickMember(team_list.last_team_id(), m, m));
	EXPECT_EQ(leader_plaeyr_id, team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_MEMBER_NOT_IN_TEAM, team_list.KickMember(team_list.last_team_id(), leader_plaeyr_id, 88));
	EXPECT_EQ(leader_plaeyr_id, team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_OK, team_list.KickMember(team_list.last_team_id(), leader_plaeyr_id, m));
	EXPECT_EQ(leader_plaeyr_id, team_list.leader_id_by_teamid(team_list.last_team_id()));

    EXPECT_EQ(1, team_list.team_size());
    EXPECT_EQ(1, team_list.players_size());
}


TEST(TeamManger, AppointLaderAndLeaveTeam1)
{
	Teams team_list;
    GameGuid m = 100;
    GameGuid leader_player_id = 100;


	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));

	GameGuid player_id = m;
	for (std::size_t i = 1; i < kMaxMemberSize; ++i)
	{
		m = (player_id + i);
		EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
		EXPECT_EQ(1 + i, team_list.member_size(team_list.last_team_id()));
	}

	EXPECT_EQ(RET_TEAM_APPOINT_SELF, team_list.AppointLeader(team_list.last_team_id(), 101, 101));
	EXPECT_EQ(leader_player_id, team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_APPOINT_SELF, team_list.AppointLeader(team_list.last_team_id(), 101, 100));
	EXPECT_EQ(leader_player_id, team_list.leader_id_by_teamid(team_list.last_team_id()));
	EXPECT_EQ(RET_TEAM_APPOINT_SELF, team_list.AppointLeader(team_list.last_team_id(), 100, 100));
	EXPECT_EQ(leader_player_id, team_list.leader_id_by_teamid(team_list.last_team_id()));

	EXPECT_EQ(RET_OK, team_list.AppointLeader(team_list.last_team_id(), 100, 101));
	EXPECT_EQ(101, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(101);

	EXPECT_EQ(leader_player_id, team_list.leader_id_by_teamid(team_list.last_team_id()));

	++leader_player_id;
	++leader_player_id;
	EXPECT_EQ(RET_OK, team_list.AppointLeader(team_list.last_team_id(), 100, 102));
	EXPECT_EQ(leader_player_id, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(102);
	leader_player_id = 100;
	EXPECT_EQ(leader_player_id, team_list.leader_id_by_teamid(team_list.last_team_id()));

	EXPECT_EQ(RET_OK, team_list.AppointLeader(team_list.last_team_id(), 100, 103));
	EXPECT_EQ(103, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(103);
	EXPECT_EQ(100, team_list.leader_id_by_teamid(team_list.last_team_id()));

	EXPECT_EQ(RET_OK, team_list.AppointLeader(team_list.last_team_id(), 100, 104));
	EXPECT_EQ(104, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(104);
	EXPECT_EQ(100, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(100);
	EXPECT_FALSE(team_list.PlayerInTeam(100));
}

TEST(TeamManger, AppointLaderAndLeaveTeam2)
{
	Teams  team_list;
	GameGuid m = 100;

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));

	m = 104;
	EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));

	EXPECT_EQ(RET_OK, team_list.AppointLeader(team_list.last_team_id(), 100, 104));
	EXPECT_EQ(104, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(100);
	EXPECT_EQ(104, team_list.leader_id_by_teamid(team_list.last_team_id()));

	team_list.LeaveTeam(104 );
	EXPECT_FALSE(team_list.PlayerInTeam(104));
}


TEST(TeamManger, DismissTeam)
{
	Teams  team_list;
	GameGuid m;
	m = (100);
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));

	m = (104);
	EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));

	EXPECT_EQ(RET_TEAM_DISMISS_NOT_LEADER, team_list.DissMissTeam(team_list.last_team_id(), 104));
	EXPECT_EQ(RET_TEAM_HAS_NOT_TEAM_ID, team_list.DissMissTeam(111, 104));
	EXPECT_EQ(RET_OK, team_list.DissMissTeam(team_list.last_team_id(), 100));
	EXPECT_FALSE(team_list.PlayerInTeam(100));
}

TEST(TeamManger, ApplyFull)
{
	Teams  team_list;
	GameGuid m =1001;

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));

	GameGuid a;

	GameGuid nMax = kMaxApplicantSize * 2;
	for (GameGuid i = 0; i < nMax; ++i)
	{
		a = i;
		EXPECT_EQ(RET_OK, team_list.ApplyForTeam(team_list.last_team_id(),  a));
		if (i < kMaxApplicantSize)
		{
			EXPECT_EQ(i + 1, team_list.applicant_size_by_team_id(team_list.last_team_id()));
		}
		else
		{
			EXPECT_EQ(kMaxApplicantSize, team_list.applicant_size_by_team_id(team_list.last_team_id()));
			EXPECT_EQ(i - kMaxApplicantSize + 1, team_list.first_applicant_id(team_list.last_team_id()));
		}
	}

	for (GameGuid i = 0; i < nMax - kMaxApplicantSize; ++i)
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
    Teams  team_list;
    GameGuid m;
    m = (1001);
    EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));
    GameGuid a;

	GameGuid nMax = kMaxApplicantSize;
    for (GameGuid i = 0; i < nMax; ++i)
    {
        a = (i);
        EXPECT_EQ(RET_OK, team_list.ApplyForTeam(team_list.last_team_id(), a));
    }
    EXPECT_EQ(nMax - kMaxApplicantSize, team_list.first_applicant_id(team_list.last_team_id()));

    for (GameGuid i = 0; i < nMax; ++i)
    {
        a = (i);
        EXPECT_EQ(RET_OK, team_list.ApplyForTeam(team_list.last_team_id(), a));
    }

    EXPECT_EQ(nMax - kMaxApplicantSize, team_list.first_applicant_id(team_list.last_team_id()));
}

TEST(TeamManger, InTeamApplyForTeam)
{
	Teams  team_list;
	GameGuid m;
	m = (1001);

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));

	GameGuid a;

	GameGuid nMax = kMaxApplicantSize;
    for (GameGuid i = 1; i < nMax; ++i)
    {
        a = (i);
        EXPECT_EQ(RET_OK, team_list.ApplyForTeam(team_list.last_team_id(), a));
    }
	for (GameGuid i = 1; i < nMax; ++i)
	{
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
    a = (6666);
    EXPECT_EQ(RET_TEAM_MEMBERS_FULL, team_list.ApplyForTeam(team_list.last_team_id(), a));

    EXPECT_EQ(RET_OK, team_list.LeaveTeam(2));

    m = (2);
    EXPECT_EQ(RET_OK, team_list.ApplyForTeam(team_list.last_team_id(), m));
    EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));
    EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.AgreeApplicant(team_list.last_team_id(), 2));
    EXPECT_FALSE(team_list.HasApplicant(team_list.last_team_id(), 2));
}


TEST(TeamManger, RemoveApplicant)
{
	Teams  team_list;
	GameGuid m;
	m = (1001);

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));
	GameGuid a;

	GameGuid nMax = kMaxMemberSize;

	GameGuid player_id = 1;
	for (GameGuid i = 0; i < nMax; ++i)
	{
		a = (player_id++);
		EXPECT_EQ(RET_OK, team_list.ApplyForTeam(team_list.last_team_id(), a));
		if (i % 2 == 0)
		{
			EXPECT_EQ(RET_OK, team_list.RemoveApplicant(team_list.last_team_id(), a));
		}
		
		if (i >= 19 && i % 2 != 0)
		{
			EXPECT_EQ(10, team_list.applicant_size_by_team_id(team_list.last_team_id()));
			EXPECT_EQ(i - 17, team_list.first_applicant_id(team_list.last_team_id()));
		}
	}

	EXPECT_TRUE(team_list.TestApplicantValueEqual(team_list.last_team_id()));
}

TEST(TeamManger, AgreeApplicant)
{
	Teams  team_list;
	GameGuid m;
	m = (1001);
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));

	GameGuid a;

	int32_t nMax = kMaxApplicantSize ;

	GameGuid player_id = 1;
    
    for (int32_t i = 0; i < nMax; ++i)
    {
        a = (i);
        EXPECT_EQ(RET_OK, team_list.ApplyForTeam(team_list.last_team_id(), a));
        if (i > kMaxApplicantSize)
        {
            EXPECT_EQ(kMaxApplicantSize, team_list.applicant_size_by_team_id(team_list.last_team_id()));
            EXPECT_EQ(i - kMaxApplicantSize + 1, team_list.first_applicant_id(team_list.last_team_id()));
        }
    }
    player_id = 0;
    GameGuid nBegiplayer_id = 1;
	for (int32_t i = 0; i < nMax; ++i)
	{
		a = (player_id++);
		
		if (i > (nMax - kMaxMemberSize ))
		{
			EXPECT_EQ(RET_OK, team_list.AgreeApplicant(team_list.last_team_id(), a));
			EXPECT_TRUE(team_list.PlayerInTheTeam(team_list.last_team_id(), a));
            if (nBegiplayer_id == 1)
            {
                nBegiplayer_id = a;
            }
            
		}
		else
		{
			EXPECT_FALSE(team_list.PlayerInTheTeam(team_list.last_team_id(), a));
		}
	}

	for (uint64_t i = nBegiplayer_id; i < nMax; ++i )
	{
		EXPECT_TRUE(team_list.PlayerInTheTeam(team_list.last_team_id(), i));
	}

	EXPECT_TRUE(team_list.TestApplicantValueEqual(team_list.last_team_id()));
}

TEST(TeamManger, PlayerTeamId)
{

	Teams  team_list;
	GameGuid m;
	m = (1);

	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));
	EXPECT_TRUE(team_list.PlayerInTeam(m));
	EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(m));

	m = (2);
	EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
	EXPECT_TRUE(team_list.PlayerInTeam(m));
	EXPECT_TRUE(team_list.PlayerInTheTeam(team_list.last_team_id(), m));
	EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(m));

	m = (3);
	EXPECT_EQ(RET_OK, team_list.ApplyForTeam(team_list.last_team_id(), m));
	EXPECT_FALSE(team_list.PlayerInTeam(m));
	EXPECT_EQ(kEmptyGameGuid, team_list.GetTeamId(m));

	EXPECT_EQ(RET_OK, team_list.RemoveApplicant(team_list.last_team_id(), m));
	EXPECT_FALSE(team_list.PlayerInTeam(m));
	EXPECT_EQ(kEmptyGameGuid, team_list.GetTeamId(m));


	EXPECT_EQ(RET_OK, team_list.ApplyForTeam(team_list.last_team_id(),  m));
	EXPECT_EQ(RET_OK, team_list.AgreeApplicant(team_list.last_team_id(), m));
	EXPECT_TRUE(team_list.PlayerInTeam(m));
	EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(m));


	for (std::size_t i = 4; i <= kMaxMemberSize; ++i)
	{
		m = (i);
		EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
		EXPECT_TRUE(team_list.PlayerInTeam(m));
		EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(m));
	}

	m = (1);
	EXPECT_EQ(RET_OK, team_list.LeaveTeam(m));
	EXPECT_FALSE(team_list.PlayerInTeam(m));
	EXPECT_EQ(kEmptyGameGuid, team_list.GetTeamId(m));
	
	EXPECT_EQ(2, team_list.leader_id_by_teamid(team_list.last_team_id()));

	m = (3);
	EXPECT_EQ(RET_OK, team_list.KickMember(team_list.last_team_id(), 2, m));
	EXPECT_FALSE(team_list.PlayerInTeam(m));
	EXPECT_EQ(kEmptyGameGuid, team_list.GetTeamId(m));
	

	EXPECT_EQ(RET_OK, team_list.DissMissTeam(team_list.last_team_id(), 2));
	for (std::size_t i = 4; i <= kMaxMemberSize; ++i)
	{
		EXPECT_FALSE(team_list.PlayerInTeam(m));
		EXPECT_EQ(kEmptyGameGuid, team_list.GetTeamId(m));
	}


    EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));

    for (std::size_t i = 4; i < kMaxMemberSize; ++i)
    {
        m = (i);
        EXPECT_EQ(RET_OK, team_list.JoinTeam(team_list.last_team_id(), m));
        EXPECT_TRUE(team_list.PlayerInTeam(m));
        EXPECT_EQ(RET_OK, team_list.LeaveTeam(m));
        EXPECT_FALSE(team_list.PlayerInTeam(m));
    }
}

TEST(TeamManger, PlayerInTeam)
{

	Teams  team_list;
	GameGuid m;
	m = (1);
	
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.CreateTeam({ m, UI64USet{m}}));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_list.last_team_id(), m));
	auto team_id1 = team_list.last_team_id();

	m = (2);
	EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_id1, m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.ApplyForTeam(team_id1, m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.AgreeApplicant(team_id1, m));
	auto team_id2 = team_list.last_team_id();

	EXPECT_EQ(RET_TEAM_HAS_NOT_TEAM_ID, team_list.LeaveTeam(kEmptyGameGuid));
	EXPECT_EQ(RET_OK, team_list.LeaveTeam(m));
	EXPECT_EQ(RET_OK, team_list.JoinTeam(team_id1, m));

	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.ApplyForTeam(team_id1, m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.AgreeApplicant(team_id1, m));

	EXPECT_EQ(RET_OK, team_list.LeaveTeam( m));
	EXPECT_EQ(RET_OK, team_list.ApplyForTeam(team_id1, m));
	EXPECT_EQ(RET_OK, team_list.AgreeApplicant(team_id1, m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_id1, m));

	//invite
	/*EXPECT_EQ(RET_OK, team_list.LeaveTeam(m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_id1, m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.ApplyForTeam(team_id1, m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.AgreeApplicant(team_id1, m));*/
}


TEST(TeamManger, AppointLeaderNotInTeam)
{
    Teams  team_list;
    GameGuid m;
    m = (1);
    GameGuid leader_player_id = 1;
    EXPECT_EQ(RET_OK, team_list.CreateTeam({ m, UI64USet{m}}));
    for (GameGuid i = leader_player_id + 1; i < 10; i++)
    {
        EXPECT_EQ(RET_TEAM_HAS_NOT_TEAM_ID, team_list.AppointLeader(team_list.last_team_id(), leader_player_id, i));
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
