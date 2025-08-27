#include <gtest/gtest.h>

#include "pbc/common_error_tip.pb.h"
#include "pbc/team_error_tip.pb.h"
#include "teams/team_system.h"
#include "thread_local/storage_common_logic.h"

TEST(TeamManger, CreateFullDismiss)
{
	TeamSystem team_list;

	typedef std::vector<Guid> PlayerIDVector;
	PlayerIDVector team_idl_ist;
	Guid player_id = 1;
	for (int32_t i = 0; i < kMaxTeamSize; ++i)
	{
		const CreateTeamParams p{player_id, UInt64Set{player_id}};
		EXPECT_EQ(kSuccess, team_list.CreateTeam(p));
		team_idl_ist.push_back(team_list.GetLastTeamId());
		++player_id;
	}

	EXPECT_TRUE(team_list.IsTeamListMax());
	player_id++;
	EXPECT_EQ(kTeamListMaxSize, team_list.CreateTeam({ player_id, UInt64Set{player_id}}));

	EXPECT_EQ(kMaxTeamSize, team_list.GetTeamSize());

	for (auto it = team_idl_ist.begin(); it != team_idl_ist.end(); ++it)
	{
		EXPECT_EQ(kSuccess, team_list.Disbanded(*it, TeamSystem::GetLeaderIdByTeamId(*it)));
	}
	EXPECT_EQ(0, team_list.GetTeamSize());
	EXPECT_EQ(0, team_list.GetPlayersSize());
}

TEST(TeamManger, TeamSizeTest)
{
	TeamSystem team_list;
	Guid member_id = 100;
	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_TRUE(team_list.HasMember(team_list.GetLastTeamId(), member_id));
	EXPECT_EQ(kTeamMemberInTeam, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
	EXPECT_EQ(1, team_list.GetMemberSize(team_list.GetLastTeamId()));

	for (std::size_t i = 1; i < kFiveMemberMaxSize; ++i)
	{
		member_id = member_id + i;
		EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
		EXPECT_EQ(1 + i, team_list.GetMemberSize(team_list.GetLastTeamId()));
	}
	++member_id;
	EXPECT_EQ(kTeamMembersFull, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
	EXPECT_EQ(kFiveMemberMaxSize, team_list.GetMemberSize(team_list.GetLastTeamId()));
}

TEST(TeamManger, LeaveTeam)
{
	TeamSystem team_list;
	constexpr Guid member_id = 100;
	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_TRUE(team_list.HasMember(team_list.GetLastTeamId(), member_id));
	EXPECT_EQ(kTeamMemberInTeam, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
	EXPECT_EQ(1, team_list.GetMemberSize(team_list.GetLastTeamId()));

	TeamSystem::LeaveTeam(member_id);
    EXPECT_FALSE(team_list.HasMember(team_list.GetLastTeamId(), member_id));
	EXPECT_EQ(0, team_list.GetMemberSize(team_list.GetLastTeamId()));
	EXPECT_EQ(kTeamHasNotTeamId, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
	EXPECT_EQ(0, team_list.GetMemberSize(team_list.GetLastTeamId()));

	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	Guid player_id = member_id;
	for (std::size_t i = 1; i < kFiveMemberMaxSize; ++i)
	{
		player_id = player_id + i;
		EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), player_id));
		EXPECT_EQ(1 + i, team_list.GetMemberSize(team_list.GetLastTeamId()));
	}

	player_id = member_id;
	for (std::size_t i = 0; i < kFiveMemberMaxSize; ++i)
	{
		player_id = player_id + i;
		TeamSystem::LeaveTeam(player_id);
        EXPECT_FALSE(team_list.HasMember(team_list.GetLastTeamId(), player_id));
		if (i < 4)
		{
			EXPECT_EQ(player_id + i + 1, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));
			EXPECT_EQ(kFiveMemberMaxSize - i - 1, team_list.GetMemberSize(team_list.GetLastTeamId()));
		}
		EXPECT_EQ(kFiveMemberMaxSize - i - 1 , team_list.GetMemberSize(team_list.GetLastTeamId()));
	}
    EXPECT_EQ(0, team_list.GetTeamSize());
    EXPECT_EQ(0, team_list.GetPlayersSize());
}


TEST(TeamManger, KickTeaamMember)
{
	TeamSystem team_list;
	Guid member_id = 100;
	constexpr Guid leader_player_id = 100;

	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	EXPECT_EQ(kTeamKickSelf, team_list.KickMember(team_list.GetLastTeamId(), member_id, member_id));
	EXPECT_EQ(kTeamKickNotLeader, team_list.KickMember(team_list.GetLastTeamId(),99, 99));

	member_id = (member_id + 1);
	EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
	EXPECT_EQ(kTeamKickSelf, team_list.KickMember(team_list.GetLastTeamId(), leader_player_id, leader_player_id));
	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));
	EXPECT_EQ(kTeamKickNotLeader, team_list.KickMember(team_list.GetLastTeamId(), member_id, leader_player_id));
	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));
	EXPECT_EQ(kTeamKickNotLeader, team_list.KickMember(team_list.GetLastTeamId(), member_id, member_id));
	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));
	EXPECT_EQ(kTeamMemberNotInTeam, team_list.KickMember(team_list.GetLastTeamId(), leader_player_id, 88));
	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));
	EXPECT_EQ(kSuccess, team_list.KickMember(team_list.GetLastTeamId(), leader_player_id, member_id));
	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

    EXPECT_EQ(1, team_list.GetTeamSize());
    EXPECT_EQ(1, team_list.GetPlayersSize());
}


TEST(TeamManger, AppointLaderAndLeaveTeam1)
{
	TeamSystem team_list;
	Guid member_id = 100;
	Guid leader_player_id = 100;

	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	Guid player_id = member_id;
	for (std::size_t i = 1; i < kFiveMemberMaxSize; ++i)
	{
		member_id = (player_id + i);
		EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
		EXPECT_EQ(1 + i, team_list.GetMemberSize(team_list.GetLastTeamId()));
	}

	EXPECT_EQ(kTeamAppointSelf, team_list.AppointLeader(team_list.GetLastTeamId(), 101, 101));
	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));
	EXPECT_EQ(kTeamAppointSelf, team_list.AppointLeader(team_list.GetLastTeamId(), 101, 100));
	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));
	EXPECT_EQ(kTeamAppointSelf, team_list.AppointLeader(team_list.GetLastTeamId(), 100, 100));
	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	EXPECT_EQ(kSuccess, team_list.AppointLeader(team_list.GetLastTeamId(), 100, 101));
	EXPECT_EQ(101, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	TeamSystem::LeaveTeam(101);

	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	++leader_player_id;
	++leader_player_id;
	EXPECT_EQ(kSuccess, team_list.AppointLeader(team_list.GetLastTeamId(), 100, 102));
	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	TeamSystem::LeaveTeam(102);
	leader_player_id = 100;
	EXPECT_EQ(leader_player_id, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	EXPECT_EQ(kSuccess, team_list.AppointLeader(team_list.GetLastTeamId(), 100, 103));
	EXPECT_EQ(103, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	TeamSystem::LeaveTeam(103);
	EXPECT_EQ(100, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	EXPECT_EQ(kSuccess, team_list.AppointLeader(team_list.GetLastTeamId(), 100, 104));
	EXPECT_EQ(104, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	TeamSystem::LeaveTeam(104);
	EXPECT_EQ(100, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	TeamSystem::LeaveTeam(100);
	EXPECT_FALSE(team_list.HasTeam(100));
}

TEST(TeamManger, AppointLaderAndLeaveTeam2)
{
	TeamSystem team_list;
	Guid member_id = 100;

	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	member_id = 104;
	EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));

	EXPECT_EQ(kSuccess, team_list.AppointLeader(team_list.GetLastTeamId(), 100, 104));
	EXPECT_EQ(104, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	TeamSystem::LeaveTeam(100);
	EXPECT_EQ(104, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	TeamSystem::LeaveTeam(104 );
	EXPECT_FALSE(team_list.HasTeam(104));
}


TEST(TeamManger, DismissTeam)
{
	TeamSystem team_list;
	Guid member_id = 100;
	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	member_id = 104;
	EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));

	EXPECT_EQ(kTeamDismissNotLeader, team_list.Disbanded(team_list.GetLastTeamId(), 104));
	EXPECT_EQ(kTeamHasNotTeamId, team_list.Disbanded(111, 104));
	EXPECT_EQ(kSuccess, team_list.Disbanded(team_list.GetLastTeamId(), 100));
	EXPECT_FALSE(team_list.HasTeam(100));
}

TEST(TeamManger, ApplyFull)
{
	TeamSystem  team_list;
	constexpr Guid member_id =1001;

	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	constexpr Guid nMax = kMaxApplicantSize * 2;
	for (Guid i = 0; i < nMax; ++i)
	{
		const Guid app = i;
		EXPECT_EQ(kSuccess, team_list.ApplyToTeam(team_list.GetLastTeamId(), app));
		if (i < kMaxApplicantSize)
		{
			EXPECT_EQ(i + 1, team_list.GetApplicantSizeByTeamId(team_list.GetLastTeamId()));
		}
		else
		{
			EXPECT_EQ(kMaxApplicantSize, team_list.GetApplicantSizeByTeamId(team_list.GetLastTeamId()));
			EXPECT_EQ(i - kMaxApplicantSize + 1, team_list.GetFirstApplicant(team_list.GetLastTeamId()));
		}
	}

	for (Guid i = 0; i < nMax - kMaxApplicantSize; ++i)
	{
		EXPECT_FALSE(team_list.IsApplicant(team_list.GetLastTeamId(), i));
	}

	for (Guid i = nMax - 10; i < nMax; ++i)
	{
		EXPECT_TRUE(team_list.IsApplicant(team_list.GetLastTeamId(), i));
	}
}

TEST(TeamManger, ApplicantOrder)
{
	TeamSystem team_list;
	Guid member_id;
	member_id = (1001);
	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	Guid a;

	constexpr Guid nMax = kMaxApplicantSize;
	for (Guid i = 0; i < nMax; ++i)
	{
		a = (i);
		EXPECT_EQ(kSuccess, team_list.ApplyToTeam(team_list.GetLastTeamId(), a));
	}
	EXPECT_EQ(nMax - kMaxApplicantSize, team_list.GetFirstApplicant(team_list.GetLastTeamId()));

	for (Guid i = 0; i < nMax; ++i)
	{
		a = (i);
		EXPECT_EQ(kSuccess, team_list.ApplyToTeam(team_list.GetLastTeamId(), a));
	}

	EXPECT_EQ(nMax - kMaxApplicantSize, team_list.GetFirstApplicant(team_list.GetLastTeamId()));
}

TEST(TeamManger, InTeamApplyForTeam)
{
	TeamSystem team_list;
	Guid member_id;
	member_id = (1001);

	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	Guid a;

	constexpr Guid nMax = kMaxApplicantSize;
	for (Guid i = 1; i < nMax; ++i)
	{
		a = (i);
		EXPECT_EQ(kSuccess, team_list.ApplyToTeam(team_list.GetLastTeamId(), a));
	}
	for (Guid i = 1; i < nMax; ++i)
	{
		if (i < kFiveMemberMaxSize)
		{
			EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), i));
			EXPECT_FALSE(team_list.IsApplicant(team_list.GetLastTeamId(), i));
		}
		else
		{
			EXPECT_EQ(kTeamMembersFull, team_list.JoinTeam(team_list.GetLastTeamId(), i));
			EXPECT_TRUE(team_list.IsApplicant(team_list.GetLastTeamId(), i));
		}
	}
	a = (6666);
	EXPECT_EQ(kTeamMembersFull, team_list.ApplyToTeam(team_list.GetLastTeamId(), a));

	EXPECT_EQ(kSuccess, team_list.LeaveTeam(2));

	member_id = (2);
	EXPECT_EQ(kSuccess, team_list.ApplyToTeam(team_list.GetLastTeamId(), member_id));
	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_EQ(kTeamMemberInTeam, team_list.JoinTeam(team_list.GetLastTeamId(), 2));
	EXPECT_FALSE(team_list.IsApplicant(team_list.GetLastTeamId(), 2));
}


TEST(TeamManger, RemoveApplicant)
{
	TeamSystem team_list;
	constexpr Guid member_id = 1001;

	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	constexpr Guid nMax = kFiveMemberMaxSize;

	Guid player_id = 1;
	for (Guid i = 0; i < nMax; ++i)
	{
		Guid app = (player_id++);
		EXPECT_EQ(kSuccess, team_list.ApplyToTeam(team_list.GetLastTeamId(), app));
		if (i % 2 == 0)
		{
			EXPECT_EQ(kSuccess, team_list.DelApplicant(team_list.GetLastTeamId(), app));
		}

		if (i >= 19 && i % 2 != 0)
		{
			EXPECT_EQ(10, team_list.GetApplicantSizeByTeamId(team_list.GetLastTeamId()));
			EXPECT_EQ(i - 17, team_list.GetFirstApplicant(team_list.GetLastTeamId()));
		}
	}

}

TEST(TeamManger, AgreeApplicant)
{
	TeamSystem team_list;
	Guid member_id;
	member_id = (1001);
	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

	Guid app = 0;

	constexpr int32_t nMax = kMaxApplicantSize;

	Guid player_id = 1;

	for (int32_t i = 0; i < nMax; ++i)
	{
		app = (i);
		EXPECT_EQ(kSuccess, team_list.ApplyToTeam(team_list.GetLastTeamId(), app));
		if (i > kMaxApplicantSize)
		{
			EXPECT_EQ(kMaxApplicantSize, team_list.GetApplicantSizeByTeamId(team_list.GetLastTeamId()));
			EXPECT_EQ(i - kMaxApplicantSize + 1, team_list.GetFirstApplicant(team_list.GetLastTeamId()));
		}
	}
	player_id = 0;
	Guid begin_player_id = 1;
	for (int32_t i = 0; i < nMax; ++i)
	{
		app = (player_id++);
		if (i > (nMax - kFiveMemberMaxSize ))
		{
			EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), app));
			EXPECT_TRUE(team_list.HasMember(team_list.GetLastTeamId(), app));
			if (begin_player_id == 1)
			{
				begin_player_id = app;
			}
		}
		else
		{
			EXPECT_FALSE(team_list.HasMember(team_list.GetLastTeamId(), app));
		}
	}

	for (uint64_t i = begin_player_id; i < nMax; ++i)
	{
		EXPECT_TRUE(team_list.HasMember(team_list.GetLastTeamId(), i));
	}
}

TEST(TeamManger, PlayerTeamId)
{
	TeamSystem team_list;
	Guid member_id;
	member_id = (1);

	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_TRUE(team_list.HasTeam(member_id));
	EXPECT_EQ(team_list.GetLastTeamId(), team_list.GetTeamId(member_id));

	member_id = (2);
	EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
	EXPECT_TRUE(team_list.HasTeam(member_id));
	EXPECT_TRUE(team_list.HasMember(team_list.GetLastTeamId(), member_id));
	EXPECT_EQ(team_list.GetLastTeamId(), team_list.GetTeamId(member_id));

	member_id = (3);
	EXPECT_EQ(kSuccess, team_list.ApplyToTeam(team_list.GetLastTeamId(), member_id));
	EXPECT_FALSE(team_list.HasTeam(member_id));
	EXPECT_EQ(kInvalidGuid, team_list.GetTeamId(member_id));

	EXPECT_EQ(kSuccess, team_list.DelApplicant(team_list.GetLastTeamId(), member_id));
	EXPECT_FALSE(team_list.HasTeam(member_id));
	EXPECT_EQ(kInvalidGuid, team_list.GetTeamId(member_id));


	EXPECT_EQ(kSuccess, team_list.ApplyToTeam(team_list.GetLastTeamId(),  member_id));
	EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
	EXPECT_TRUE(team_list.HasTeam(member_id));
	EXPECT_EQ(team_list.GetLastTeamId(), team_list.GetTeamId(member_id));


	for (std::size_t i = 4; i <= kFiveMemberMaxSize; ++i)
	{
		member_id = (i);
		EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
		EXPECT_TRUE(team_list.HasTeam(member_id));
		EXPECT_EQ(team_list.GetLastTeamId(), team_list.GetTeamId(member_id));
	}

	member_id = (1);
	EXPECT_EQ(kSuccess, team_list.LeaveTeam(member_id));
	EXPECT_FALSE(team_list.HasTeam(member_id));
	EXPECT_EQ(kInvalidGuid, team_list.GetTeamId(member_id));

	EXPECT_EQ(2, team_list.GetLeaderIdByTeamId(team_list.GetLastTeamId()));

	member_id = (3);
	EXPECT_EQ(kSuccess, team_list.KickMember(team_list.GetLastTeamId(), 2, member_id));
	EXPECT_FALSE(team_list.HasTeam(member_id));
	EXPECT_EQ(kInvalidGuid, team_list.GetTeamId(member_id));

	EXPECT_EQ(kSuccess, team_list.Disbanded(team_list.GetLastTeamId(), 2));
	for (std::size_t i = 4; i <= kFiveMemberMaxSize; ++i)
	{
		EXPECT_FALSE(team_list.HasTeam(member_id));
		EXPECT_EQ(kInvalidGuid, team_list.GetTeamId(member_id));
	}


    EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));

    for (std::size_t i = 4; i < kFiveMemberMaxSize; ++i)
    {
	    member_id = (i);
	    EXPECT_EQ(kSuccess, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
	    EXPECT_TRUE(team_list.HasTeam(member_id));
	    EXPECT_EQ(kSuccess, team_list.LeaveTeam(member_id));
	    EXPECT_FALSE(team_list.HasTeam(member_id));
    }
}

TEST(TeamManger, PlayerInTeam)
{
	TeamSystem team_list;
	Guid member_id = (1);

	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_EQ(kTeamMemberInTeam, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_EQ(kTeamMemberInTeam, team_list.JoinTeam(team_list.GetLastTeamId(), member_id));
	const auto team_id1 = team_list.GetLastTeamId();

	member_id = (2);
	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	EXPECT_EQ(kTeamMemberInTeam, team_list.JoinTeam(team_id1, member_id));
	EXPECT_EQ(kTeamMemberInTeam, team_list.ApplyToTeam(team_id1, member_id));
	EXPECT_EQ(kTeamMemberInTeam, team_list.JoinTeam(team_id1, member_id));

	EXPECT_EQ(kTeamHasNotTeamId, team_list.LeaveTeam(kInvalidGuid));
	EXPECT_EQ(kSuccess, team_list.LeaveTeam(member_id));
	EXPECT_EQ(kSuccess, team_list.JoinTeam(team_id1, member_id));

	EXPECT_EQ(kTeamMemberInTeam, team_list.ApplyToTeam(team_id1, member_id));
	EXPECT_EQ(kTeamMemberInTeam, team_list.JoinTeam(team_id1, member_id));

	EXPECT_EQ(kSuccess, team_list.LeaveTeam( member_id));
	EXPECT_EQ(kSuccess, team_list.ApplyToTeam(team_id1, member_id));
	EXPECT_EQ(kSuccess, team_list.JoinTeam(team_id1, member_id));
	EXPECT_EQ(kTeamMemberInTeam, team_list.JoinTeam(team_id1, member_id));

	//invite
	/*EXPECT_EQ(kSuccess, team_list.LeaveTeam(m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.JoinTeam(team_id1, m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.ApplyForTeam(team_id1, m));
	EXPECT_EQ(RET_TEAM_MEMBER_IN_TEAM, team_list.AgreeApplicant(team_id1, m));*/
}


TEST(TeamManger, AppointLeaderNotInTeam)
{
	TeamSystem team_list;
	constexpr Guid member_id = (1);
	constexpr Guid leader_player_id = 1;
	EXPECT_EQ(kSuccess, team_list.CreateTeam({ member_id, UInt64Set{member_id}}));
	for (Guid i = leader_player_id + 1; i < 10; i++)
	{
		EXPECT_EQ(kTeamHasNotTeamId, team_list.AppointLeader(team_list.GetLastTeamId(), leader_player_id, i));
	}
}

int main(int argc, char** argv)
{
	for (size_t i = 0; i < 2000; ++i)
	{
		tlsCommonLogic.GetPlayerList().emplace(i, tls.registry.create());
	}
	testing::InitGoogleTest(&argc, argv);

	/*while (true)
	{
	    RUN_ALL_TESTS();
	}*/
	return RUN_ALL_TESTS();
}
