#include <gtest/gtest.h>

#include "src/game_logic/tips_id.h"
#include "src/game_logic/teams/TeamSystem.h"
#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"

TEST(TeamManger, CreateFullDismiss)
{
	TeamSystem team_list;

	typedef std::vector<Guid> PlayerIdsV;
	PlayerIdsV team_idl_ist;
	Guid player_id = 1;
	for (int32_t i = 0; i < kMaxTeamSize; ++i)
	{
		const CreateTeamP p{player_id, UInt64Set{player_id}};
		EXPECT_EQ(kRetOK, team_list.CreateTeam(p));
		team_idl_ist.push_back(team_list.last_team_id());
		++player_id;
	}

	EXPECT_TRUE(team_list.IsTeamListMax());
	player_id++;
	EXPECT_EQ(kRetTeamListMaxSize, team_list.CreateTeam({ player_id, UInt64Set{player_id}}));

	EXPECT_EQ(kMaxTeamSize, team_list.team_size());

	for (auto it = team_idl_ist.begin(); it != team_idl_ist.end(); ++it)
	{
		EXPECT_EQ(kRetOK, team_list.Disbanded(*it, TeamSystem::get_leader_id_by_team_id(*it)));
	}
	EXPECT_EQ(0, team_list.team_size());
	EXPECT_EQ(0, team_list.players_size());
}

TEST(TeamManger, TeamSizeTest)
{
	TeamSystem team_list;
	Guid member_id = 100;
	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_TRUE(team_list.HasMember(team_list.last_team_id(), member_id));
	EXPECT_EQ(kRetTeamMemberInTeam, team_list.JoinTeam(team_list.last_team_id(), member_id));
	EXPECT_EQ(1, team_list.member_size(team_list.last_team_id()));

	for (std::size_t i = 1; i < kFiveMemberMaxSize; ++i)
	{
		member_id = member_id + i;
		EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), member_id));
		EXPECT_EQ(1 + i, team_list.member_size(team_list.last_team_id()));
	}
	++member_id;
	EXPECT_EQ(kRetTeamMembersFull, team_list.JoinTeam(team_list.last_team_id(), member_id));
	EXPECT_EQ(kFiveMemberMaxSize, team_list.member_size(team_list.last_team_id()));
}

TEST(TeamManger, LeaveTeam)
{
	TeamSystem team_list;
	constexpr Guid member_id = 100;
	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_TRUE(team_list.HasMember(team_list.last_team_id(), member_id));
	EXPECT_EQ(kRetTeamMemberInTeam, team_list.JoinTeam(team_list.last_team_id(), member_id));
	EXPECT_EQ(1, team_list.member_size(team_list.last_team_id()));

	TeamSystem::LeaveTeam(member_id);
    EXPECT_FALSE(team_list.HasMember(team_list.last_team_id(), member_id));
	EXPECT_EQ(0, team_list.member_size(team_list.last_team_id()));
	EXPECT_EQ(kRetTeamHasNotTeamId, team_list.JoinTeam(team_list.last_team_id(), member_id));
	EXPECT_EQ(0, team_list.member_size(team_list.last_team_id()));

	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	Guid player_id = member_id;
	for (std::size_t i = 1; i < kFiveMemberMaxSize; ++i)
	{
		player_id = player_id + i;
		EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), player_id));
		EXPECT_EQ(1 + i, team_list.member_size(team_list.last_team_id()));
	}

	player_id = member_id;
	for (std::size_t i = 0; i < kFiveMemberMaxSize; ++i)
	{
		player_id = player_id + i;
		TeamSystem::LeaveTeam(player_id);
        EXPECT_FALSE(team_list.HasMember(team_list.last_team_id(), player_id));
		if (i < 4)
		{
			EXPECT_EQ(player_id + i + 1, team_list.get_leader_id_by_team_id(team_list.last_team_id()));
			EXPECT_EQ(kFiveMemberMaxSize - i - 1, team_list.member_size(team_list.last_team_id()));
		}
		EXPECT_EQ(kFiveMemberMaxSize - i - 1 , team_list.member_size(team_list.last_team_id()));
	}
    EXPECT_EQ(0, team_list.team_size());
    EXPECT_EQ(0, team_list.players_size());
}


TEST(TeamManger, KickTeaamMember)
{
	TeamSystem team_list;
	Guid member_id = 100;
	constexpr Guid leader_player_id = 100;

	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	EXPECT_EQ(kRetTeamKickSelf, team_list.KickMember(team_list.last_team_id(), member_id, member_id));
	EXPECT_EQ(kRetTeamKickNotLeader, team_list.KickMember(team_list.last_team_id(),99, 99));

	member_id = (member_id + 1);
	EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), member_id));
	EXPECT_EQ(kRetTeamKickSelf, team_list.KickMember(team_list.last_team_id(), leader_player_id, leader_player_id));
	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));
	EXPECT_EQ(kRetTeamKickNotLeader, team_list.KickMember(team_list.last_team_id(), member_id, leader_player_id));
	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));
	EXPECT_EQ(kRetTeamKickNotLeader, team_list.KickMember(team_list.last_team_id(), member_id, member_id));
	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));
	EXPECT_EQ(kRetTeamMemberNotInTeam, team_list.KickMember(team_list.last_team_id(), leader_player_id, 88));
	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));
	EXPECT_EQ(kRetOK, team_list.KickMember(team_list.last_team_id(), leader_player_id, member_id));
	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

    EXPECT_EQ(1, team_list.team_size());
    EXPECT_EQ(1, team_list.players_size());
}


TEST(TeamManger, AppointLaderAndLeaveTeam1)
{
	TeamSystem team_list;
	Guid member_id = 100;
	Guid leader_player_id = 100;

	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	Guid player_id = member_id;
	for (std::size_t i = 1; i < kFiveMemberMaxSize; ++i)
	{
		member_id = (player_id + i);
		EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), member_id));
		EXPECT_EQ(1 + i, team_list.member_size(team_list.last_team_id()));
	}

	EXPECT_EQ(kRetTeamAppointSelf, team_list.AppointLeader(team_list.last_team_id(), 101, 101));
	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));
	EXPECT_EQ(kRetTeamAppointSelf, team_list.AppointLeader(team_list.last_team_id(), 101, 100));
	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));
	EXPECT_EQ(kRetTeamAppointSelf, team_list.AppointLeader(team_list.last_team_id(), 100, 100));
	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	EXPECT_EQ(kRetOK, team_list.AppointLeader(team_list.last_team_id(), 100, 101));
	EXPECT_EQ(101, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	TeamSystem::LeaveTeam(101);

	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	++leader_player_id;
	++leader_player_id;
	EXPECT_EQ(kRetOK, team_list.AppointLeader(team_list.last_team_id(), 100, 102));
	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	TeamSystem::LeaveTeam(102);
	leader_player_id = 100;
	EXPECT_EQ(leader_player_id, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	EXPECT_EQ(kRetOK, team_list.AppointLeader(team_list.last_team_id(), 100, 103));
	EXPECT_EQ(103, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	TeamSystem::LeaveTeam(103);
	EXPECT_EQ(100, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	EXPECT_EQ(kRetOK, team_list.AppointLeader(team_list.last_team_id(), 100, 104));
	EXPECT_EQ(104, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	TeamSystem::LeaveTeam(104);
	EXPECT_EQ(100, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	TeamSystem::LeaveTeam(100);
	EXPECT_FALSE(team_list.HasTeam(100));
}

TEST(TeamManger, AppointLaderAndLeaveTeam2)
{
	TeamSystem team_list;
	Guid member_id = 100;

	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	member_id = 104;
	EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), member_id));

	EXPECT_EQ(kRetOK, team_list.AppointLeader(team_list.last_team_id(), 100, 104));
	EXPECT_EQ(104, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	TeamSystem::LeaveTeam(100);
	EXPECT_EQ(104, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	TeamSystem::LeaveTeam(104 );
	EXPECT_FALSE(team_list.HasTeam(104));
}


TEST(TeamManger, DismissTeam)
{
	TeamSystem team_list;
	Guid member_id = 100;
	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	member_id = 104;
	EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), member_id));

	EXPECT_EQ(kRetTeamDismissNotLeader, team_list.Disbanded(team_list.last_team_id(), 104));
	EXPECT_EQ(kRetTeamHasNotTeamId, team_list.Disbanded(111, 104));
	EXPECT_EQ(kRetOK, team_list.Disbanded(team_list.last_team_id(), 100));
	EXPECT_FALSE(team_list.HasTeam(100));
}

TEST(TeamManger, ApplyFull)
{
	TeamSystem  team_list;
	constexpr Guid member_id =1001;

	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	constexpr Guid nMax = kMaxApplicantSize * 2;
	for (Guid i = 0; i < nMax; ++i)
	{
		const Guid app = i;
		EXPECT_EQ(kRetOK, team_list.ApplyToTeam(team_list.last_team_id(), app));
		if (i < kMaxApplicantSize)
		{
			EXPECT_EQ(i + 1, team_list.applicant_size_by_team_id(team_list.last_team_id()));
		}
		else
		{
			EXPECT_EQ(kMaxApplicantSize, team_list.applicant_size_by_team_id(team_list.last_team_id()));
			EXPECT_EQ(i - kMaxApplicantSize + 1, team_list.first_applicant(team_list.last_team_id()));
		}
	}

	for (Guid i = 0; i < nMax - kMaxApplicantSize; ++i)
	{
		EXPECT_FALSE(team_list.IsApplicant(team_list.last_team_id(), i));
	}

	for (Guid i = nMax - 10; i < nMax; ++i)
	{
		EXPECT_TRUE(team_list.IsApplicant(team_list.last_team_id(), i));
	}
}

TEST(TeamManger, ApplicantOrder)
{
	TeamSystem team_list;
	Guid member_id;
	member_id = (1001);
	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	Guid a;

	constexpr Guid nMax = kMaxApplicantSize;
	for (Guid i = 0; i < nMax; ++i)
	{
		a = (i);
		EXPECT_EQ(kRetOK, team_list.ApplyToTeam(team_list.last_team_id(), a));
	}
	EXPECT_EQ(nMax - kMaxApplicantSize, team_list.first_applicant(team_list.last_team_id()));

	for (Guid i = 0; i < nMax; ++i)
	{
		a = (i);
		EXPECT_EQ(kRetOK, team_list.ApplyToTeam(team_list.last_team_id(), a));
	}

	EXPECT_EQ(nMax - kMaxApplicantSize, team_list.first_applicant(team_list.last_team_id()));
}

TEST(TeamManger, InTeamApplyForTeam)
{
	TeamSystem team_list;
	Guid member_id;
	member_id = (1001);

	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	Guid a;

	constexpr Guid nMax = kMaxApplicantSize;
	for (Guid i = 1; i < nMax; ++i)
	{
		a = (i);
		EXPECT_EQ(kRetOK, team_list.ApplyToTeam(team_list.last_team_id(), a));
	}
	for (Guid i = 1; i < nMax; ++i)
	{
		if (i < kFiveMemberMaxSize)
		{
			EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), i));
			EXPECT_FALSE(team_list.IsApplicant(team_list.last_team_id(), i));
		}
		else
		{
			EXPECT_EQ(kRetTeamMembersFull, team_list.JoinTeam(team_list.last_team_id(), i));
			EXPECT_TRUE(team_list.IsApplicant(team_list.last_team_id(), i));
		}
	}
	a = (6666);
	EXPECT_EQ(kRetTeamMembersFull, team_list.ApplyToTeam(team_list.last_team_id(), a));

	EXPECT_EQ(kRetOK, team_list.LeaveTeam(2));

	member_id = (2);
	EXPECT_EQ(kRetOK, team_list.ApplyToTeam(team_list.last_team_id(), member_id));
	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_EQ(kRetTeamMemberInTeam, team_list.JoinTeam(team_list.last_team_id(), 2));
	EXPECT_FALSE(team_list.IsApplicant(team_list.last_team_id(), 2));
}


TEST(TeamManger, RemoveApplicant)
{
	TeamSystem team_list;
	constexpr Guid member_id = 1001;

	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	constexpr Guid nMax = kFiveMemberMaxSize;

	Guid player_id = 1;
	for (Guid i = 0; i < nMax; ++i)
	{
		Guid app = (player_id++);
		EXPECT_EQ(kRetOK, team_list.ApplyToTeam(team_list.last_team_id(), app));
		if (i % 2 == 0)
		{
			EXPECT_EQ(kRetOK, team_list.DelApplicant(team_list.last_team_id(), app));
		}

		if (i >= 19 && i % 2 != 0)
		{
			EXPECT_EQ(10, team_list.applicant_size_by_team_id(team_list.last_team_id()));
			EXPECT_EQ(i - 17, team_list.first_applicant(team_list.last_team_id()));
		}
	}

}

TEST(TeamManger, AgreeApplicant)
{
	TeamSystem team_list;
	Guid member_id;
	member_id = (1001);
	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	Guid app = 0;

	constexpr int32_t nMax = kMaxApplicantSize;

	Guid player_id = 1;

	for (int32_t i = 0; i < nMax; ++i)
	{
		app = (i);
		EXPECT_EQ(kRetOK, team_list.ApplyToTeam(team_list.last_team_id(), app));
		if (i > kMaxApplicantSize)
		{
			EXPECT_EQ(kMaxApplicantSize, team_list.applicant_size_by_team_id(team_list.last_team_id()));
			EXPECT_EQ(i - kMaxApplicantSize + 1, team_list.first_applicant(team_list.last_team_id()));
		}
	}
	player_id = 0;
	Guid begin_player_id = 1;
	for (int32_t i = 0; i < nMax; ++i)
	{
		app = (player_id++);
		if (i > (nMax - kFiveMemberMaxSize ))
		{
			EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), app));
			EXPECT_TRUE(team_list.HasMember(team_list.last_team_id(), app));
			if (begin_player_id == 1)
			{
				begin_player_id = app;
			}
		}
		else
		{
			EXPECT_FALSE(team_list.HasMember(team_list.last_team_id(), app));
		}
	}

	for (uint64_t i = begin_player_id; i < nMax; ++i)
	{
		EXPECT_TRUE(team_list.HasMember(team_list.last_team_id(), i));
	}
}

TEST(TeamManger, PlayerTeamId)
{
	TeamSystem team_list;
	Guid member_id;
	member_id = (1);

	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_TRUE(team_list.HasTeam(member_id));
	EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(member_id));

	member_id = (2);
	EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), member_id));
	EXPECT_TRUE(team_list.HasTeam(member_id));
	EXPECT_TRUE(team_list.HasMember(team_list.last_team_id(), member_id));
	EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(member_id));

	member_id = (3);
	EXPECT_EQ(kRetOK, team_list.ApplyToTeam(team_list.last_team_id(), member_id));
	EXPECT_FALSE(team_list.HasTeam(member_id));
	EXPECT_EQ(kInvalidGuid, team_list.GetTeamId(member_id));

	EXPECT_EQ(kRetOK, team_list.DelApplicant(team_list.last_team_id(), member_id));
	EXPECT_FALSE(team_list.HasTeam(member_id));
	EXPECT_EQ(kInvalidGuid, team_list.GetTeamId(member_id));


	EXPECT_EQ(kRetOK, team_list.ApplyToTeam(team_list.last_team_id(),  member_id));
	EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), member_id));
	EXPECT_TRUE(team_list.HasTeam(member_id));
	EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(member_id));


	for (std::size_t i = 4; i <= kFiveMemberMaxSize; ++i)
	{
		member_id = (i);
		EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), member_id));
		EXPECT_TRUE(team_list.HasTeam(member_id));
		EXPECT_EQ(team_list.last_team_id(), team_list.GetTeamId(member_id));
	}

	member_id = (1);
	EXPECT_EQ(kRetOK, team_list.LeaveTeam(member_id));
	EXPECT_FALSE(team_list.HasTeam(member_id));
	EXPECT_EQ(kInvalidGuid, team_list.GetTeamId(member_id));

	EXPECT_EQ(2, team_list.get_leader_id_by_team_id(team_list.last_team_id()));

	member_id = (3);
	EXPECT_EQ(kRetOK, team_list.KickMember(team_list.last_team_id(), 2, member_id));
	EXPECT_FALSE(team_list.HasTeam(member_id));
	EXPECT_EQ(kInvalidGuid, team_list.GetTeamId(member_id));

	EXPECT_EQ(kRetOK, team_list.Disbanded(team_list.last_team_id(), 2));
	for (std::size_t i = 4; i <= kFiveMemberMaxSize; ++i)
	{
		EXPECT_FALSE(team_list.HasTeam(member_id));
		EXPECT_EQ(kInvalidGuid, team_list.GetTeamId(member_id));
	}


    EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

    for (std::size_t i = 4; i < kFiveMemberMaxSize; ++i)
    {
	    member_id = (i);
	    EXPECT_EQ(kRetOK, team_list.JoinTeam(team_list.last_team_id(), member_id));
	    EXPECT_TRUE(team_list.HasTeam(member_id));
	    EXPECT_EQ(kRetOK, team_list.LeaveTeam(member_id));
	    EXPECT_FALSE(team_list.HasTeam(member_id));
    }
}

TEST(TeamManger, PlayerInTeam)
{
	TeamSystem team_list;
	Guid member_id = (1);

	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_EQ(kRetTeamMemberInTeam, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_EQ(kRetTeamMemberInTeam, team_list.JoinTeam(team_list.last_team_id(), member_id));
	const auto team_id1 = team_list.last_team_id();

	member_id = (2);
	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_EQ(kRetTeamMemberInTeam, team_list.JoinTeam(team_id1, member_id));
	EXPECT_EQ(kRetTeamMemberInTeam, team_list.ApplyToTeam(team_id1, member_id));
	EXPECT_EQ(kRetTeamMemberInTeam, team_list.JoinTeam(team_id1, member_id));

	EXPECT_EQ(kRetTeamHasNotTeamId, team_list.LeaveTeam(kInvalidGuid));
	EXPECT_EQ(kRetOK, team_list.LeaveTeam(member_id));
	EXPECT_EQ(kRetOK, team_list.JoinTeam(team_id1, member_id));

	EXPECT_EQ(kRetTeamMemberInTeam, team_list.ApplyToTeam(team_id1, member_id));
	EXPECT_EQ(kRetTeamMemberInTeam, team_list.JoinTeam(team_id1, member_id));

	EXPECT_EQ(kRetOK, team_list.LeaveTeam( member_id));
	EXPECT_EQ(kRetOK, team_list.ApplyToTeam(team_id1, member_id));
	EXPECT_EQ(kRetOK, team_list.JoinTeam(team_id1, member_id));
	EXPECT_EQ(kRetTeamMemberInTeam, team_list.JoinTeam(team_id1, member_id));

	//invite
	/*EXPECT_EQ(kRetOK, team_list.LeaveTeam(m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_id1, m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.ApplyForTeam(team_id1, m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.AgreeApplicant(team_id1, m));*/
}


TEST(TeamManger, AppointLeaderNotInTeam)
{
	TeamSystem team_list;
	constexpr Guid member_id = (1);
	constexpr Guid leader_player_id = 1;
	EXPECT_EQ(kRetOK, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	for (Guid i = leader_player_id + 1; i < 10; i++)
	{
		EXPECT_EQ(kRetTeamHasNotTeamId, team_list.AppointLeader(team_list.last_team_id(), leader_player_id, i));
	}
}

int main(int argc, char** argv)
{
	for (size_t i = 0; i < 2000; ++i)
	{
		cl_tls.player_list().emplace(i, tls.registry.create());
	}
	testing::InitGoogleTest(&argc, argv);

	/*while (true)
	{
	    RUN_ALL_TESTS();
	}*/
	return RUN_ALL_TESTS();
}
