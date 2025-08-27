#include <gtest/gtest.h>

#include <vector>
#include <random>
#include "CommonLogic/Team/TeamList.h"

#include "CommonLogic/Team/MatchMakingManager.h"
#include "GenCode/Config/MatchCfg.h"
#include "GenCode/GameDefine_Result.h"

using namespace GameMMR;

#define INVALID_ID -1

std::random_device rd;
std::mt19937 gen(rd());

muduo::net::EventLoop * pLoop = NULL;

TEST(MatchMaking, TeamFullExitMatch)
{
    TeamList teamList;
    MatchManager o(&teamList);
    

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    MatchUnit::player_integral_map_type iv;

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));
    o.Match(E_NOMAL_TEAM_MATCH, p);
    for (INT i = 0; i < 3; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
        EXPECT_TRUE(o.InMatch(teamMem.roleid()));
    }
    for (INT i = 0; i < 1; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
    }
    

    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);

    for (int32_t i = 0; i < t.members_size(); ++i)
    {
        EXPECT_FALSE(o.InMatch(t.members(i).roleid()));
        
    }
}



TEST(MatchMaking, MatchChangeProfession)
{
    TeamList teamList;
    MatchManager o(&teamList);
    o.SetWithoutRepetitionProfession(true);

    TeamTarget  target;
    for (int32_t i = 1; i < 4; ++i)
    {
        TeamProfessionTarget* pp = target.add_profession();
        pp->set_professionid(i);
        pp->set_size(1);
    }

    GUID_t nRoleId = 1;
    typedef std::unordered_set<GUID_t> rolesets;
    typedef std::vector<GUID_t> rolevec;
    rolevec vecRoles;
    rolesets vRoles;
    std::vector<Team::team_id_type> teamids;

    std::uniform_int_distribution<int32_t> prefessiondis(1, 3);
    std::size_t rolesize = vRoles.size();
    const MatchElement* pMatchElement = MatchTable::Instance().GetElement(E_MATCH_15V15);
    std::uniform_int_distribution<int32_t> sectiondis(pMatchElement->sections_list[0], pMatchElement->sections_list[pMatchElement->sections_list.size() - 1]);
    rolevec exitrole;


    for (int32_t jjj = 1; jjj < 10; ++jjj)
    {

        TeamMemberInfo teamMem;
        teamMem.set_roleid(++nRoleId);
        teamMem.set_professionid(1);
        vRoles.emplace(nRoleId);
        vecRoles.push_back(nRoleId);
        Team::team_id_type nTeamTypeTeamId1 = INVALID_ID;
        EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId1, target));


        teamMem.set_roleid(++nRoleId);
        teamMem.set_professionid(2);
        vRoles.emplace(nRoleId);
        vecRoles.push_back(nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);

        teamMem.set_roleid(++nRoleId);
        teamMem.set_professionid(2);
        vRoles.emplace(nRoleId);
        vecRoles.push_back(nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);


        teamMem.set_roleid(++nRoleId);
        teamMem.set_professionid(2);
        vRoles.emplace(nRoleId);
        vecRoles.push_back(nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);

        TeamInfo t;
        o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
        std::size_t old_size = 4;
        MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));
        EXPECT_EQ(old_size, o.GetTeamList().GetTeamRoleIds(nTeamTypeTeamId1).size());

        int32_t ret = o.Match(E_NOMAL_TEAM_MATCH, p);


        teamids.push_back(nTeamTypeTeamId1);

        MatchInfo info;
        info.set_roleid(++nRoleId);
        info.set_professionid(1);
        vRoles.emplace(nRoleId);
        vecRoles.push_back(nRoleId);

        MatchMaking::match_unit_type p4(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
        EXPECT_EQ(OR_OK, o.Match(E_NOMAL_TEAM_MATCH, p4));

        EXPECT_EQ(old_size, o.GetTeamList().GetTeamRoleIds(nTeamTypeTeamId1).size());
        o.ChangeProfession(nRoleId, 3);
        EXPECT_EQ(5, o.GetTeamList().GetTeamRoleIds(nTeamTypeTeamId1).size());
        EXPECT_TRUE(o.GetTeamList().InTeam(nTeamTypeTeamId1, nRoleId));
    }



}


TEST(MatchMaking, TeamMatchLeaveTeam)
{
    TeamList teamList;
    MatchManager o(&teamList);


    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    MatchUnit::player_integral_map_type iv;

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));
    o.Match(E_NOMAL_TEAM_MATCH, p);
    MatchInfo info;
    info.set_roleid(++nRoleId);
    MatchMaking::match_unit_type p1(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
    o.Match(E_NOMAL_TEAM_MATCH, p1);
    EXPECT_TRUE(o.InMatch(nRoleId));
    EXPECT_EQ(OR_OK, o.GetTeamList().LeaveTeam(nTeamTypeTeamId, nRoleId));
   
    EXPECT_FALSE(o.InMatch(nRoleId));

}

TEST(MatchMaking, TeamMatchLeaveTeamGoOnMatch)
{
    TeamList teamList;
    MatchManager o(&teamList);


    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    MatchUnit::player_integral_map_type iv;

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));
    o.Match(E_NOMAL_TEAM_MATCH, p);
    MatchInfo info;
    info.set_roleid(++nRoleId);
    for (int32_t i = 0; i < 20; ++i)
    {
        MatchMaking::match_unit_type p1(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
        o.Match(E_NOMAL_TEAM_MATCH, p1);
        EXPECT_TRUE(o.GetTeamList().InTeam(nTeamTypeTeamId, nRoleId));
        EXPECT_TRUE(o.InMatch(nRoleId));
        EXPECT_EQ(OR_OK, o.GetTeamList().LeaveTeam(nTeamTypeTeamId, nRoleId));
        EXPECT_FALSE(o.InMatch(nRoleId));
    }
}

TEST(MatchMaking, ProfessionSize1)
{
    TeamList teamList;
    MatchManager o(&teamList);
    o.SetWithoutRepetitionProfession(true);

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    int32_t professionid = 1;
    teamMem.set_professionid(professionid);
    MatchUnit::player_integral_map_type iv;

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    TeamTarget  target;
    for (int32_t i = 1; i < 4; ++i)
    {
        TeamProfessionTarget* pp = target.add_profession();
        pp->set_professionid(i);
        pp->set_size(1);
    }
    
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId, target));

    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));
    o.Match(E_NOMAL_TEAM_MATCH, p);
    
    
    for (int32_t i = 0; i < 5; ++i)
    {
        MatchInfo info;
        info.set_roleid(++nRoleId);
        info.set_professionid(professionid);
        MatchMaking::match_unit_type p1(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
        o.Match(E_NOMAL_TEAM_MATCH, p1);
    }
    
    for (int32_t i = 0; i < 2; ++i)
    {
        MatchInfo info;
        info.set_roleid(++nRoleId);
        info.set_professionid(++professionid);
        MatchMaking::match_unit_type p1(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
        o.Match(E_NOMAL_TEAM_MATCH, p1);
    }


    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    for (int32_t i = 0; i < 3; ++i)
    {
        EXPECT_EQ(1, t.members(i).professionid());
    }
    for (int32_t i = 3; i < 5; ++i)
    {
        EXPECT_EQ(i - 1, t.members(i).professionid());
    }
}

TEST(MatchMaking, ProfessionSize2)
{
    GUID_t nRoleId = 1;
    for (int32_t rj = 0; rj < 10; ++rj)
    {
        TeamList teamList;
        MatchManager o(&teamList);
        o.SetWithoutRepetitionProfession(true);

        
        TeamMemberInfo teamMem;
        teamMem.set_roleid(++nRoleId);
        int32_t professionid = 1;
        
        MatchUnit::player_integral_map_type iv;

        Team::team_id_type nTeamTypeTeamId = INVALID_ID;
        TeamTarget  target;

        std::uniform_int_distribution<int32_t> dis(1, 4);

        std::uniform_int_distribution<int32_t> dis1(0, 1);
        int32_t teamType = dis1(gen);

        int32_t ps = dis(gen);

        std::size_t teamMaxSize = Team::GetMaxSize(teamType);
        std::size_t teamMaxSize2 = teamMaxSize * 2;

        for (int32_t i = 1; i < ps; ++i)
        {
            TeamProfessionTarget* pp = target.add_profession();
            pp->set_professionid(i);

            std::uniform_int_distribution<int32_t> disSize(0, teamMaxSize);
            int32_t s = disSize(gen);
            pp->set_size(s);
            if (s > 0 )
            {
                professionid = i;
            }
            teamMaxSize -= s;
        }
        teamMem.set_professionid(professionid);
        EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, teamType, nTeamTypeTeamId, target));

        TeamInfo t;
        o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
        MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));
        o.Match(E_NOMAL_TEAM_MATCH, p);


        std::uniform_int_distribution<int32_t> dismatch(1, 2);
        int32_t rm = dismatch(gen);
        if (rm == 1)
        {
            for (int32_t i = 0; i < target.profession_size(); ++i)
            {
                for (int32_t j = 0; j < target.profession(i).size(); ++j)
                {
                    MatchInfo info;
                    info.set_roleid(++nRoleId);
                    info.set_professionid(target.profession(i).professionid());
                    MatchMaking::match_unit_type p1(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
                    o.Match(E_NOMAL_TEAM_MATCH, p1);
                }
            }

            for (size_t i = 0; i < teamMaxSize2; ++i)
            {
                MatchInfo info;
                info.set_roleid(++nRoleId);
                info.set_professionid(professionid);
                MatchMaking::match_unit_type p1(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
                o.Match(E_NOMAL_TEAM_MATCH, p1);
            }

           
        }
        else
        {
            for (size_t i = 0; i < teamMaxSize2; ++i)
            {
                MatchInfo info;
                info.set_roleid(++nRoleId);
                info.set_professionid(professionid);
                MatchMaking::match_unit_type p1(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
                o.Match(E_NOMAL_TEAM_MATCH, p1);
            }

            for (int32_t i = 0; i < target.profession_size(); ++i)
            {
                for (int32_t j = 0; j < target.profession(i).size(); ++j)
                {
                    MatchInfo info;
                    info.set_roleid(++nRoleId);
                    info.set_professionid(target.profession(i).professionid());
                    MatchMaking::match_unit_type p1(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
                    o.Match(E_NOMAL_TEAM_MATCH, p1);
                }
            }

           
        }

        o.GetTeamList().CopyTo(nTeamTypeTeamId, t);

        if (ps == 1)
        {
            

            EXPECT_EQ(teamMaxSize2 / 2, t.members_size());
        }
        else
        {
            i32_map_type v;

            for (int32_t i = 0; i < t.members_size(); ++i)
            {
                i32_map_type::iterator it = v.find(t.members(i).professionid());
                if (it == v.end())
                {
                    v.emplace(t.members(i).professionid(), 1);
                }
                else
                {
                    ++it->second;
                }
            
            }

            for (int32_t ti = 0; ti < target.profession_size(); ++ti)
            {
                bool b = v[target.profession(ti).professionid()] >= target.profession(ti).size();
                if (!b)
                {
                    target.PrintDebugString();
                    std::cout << "---------------" << std::endl;
                    t.PrintDebugString();
                }
                EXPECT_TRUE(b);
            }
        }
        
    }
   
}

TEST(MatchMaking, JoinTeamCancelMatch_AgreeApply)
{
    TeamList teamList;
    MatchManager o(&teamList);
    

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    MatchUnit::player_integral_map_type iv;

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

 
    teamMem.set_roleid(++nRoleId);

    o.GetTeamList().Apply(nTeamTypeTeamId, teamMem);
   
    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);

    MatchInfo info;
    info.set_roleid(teamMem.roleid());
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));

    o.Match(E_NOMAL_TEAM_MATCH, p);
    EXPECT_TRUE(o.InMatch(teamMem.roleid()));
    o.GetTeamList().AgreeApplicant(nTeamTypeTeamId, teamMem.roleid());
    EXPECT_FALSE(o.InMatch(teamMem.roleid()));


}

TEST(MatchMaking, CreateTeamCancelMatch)
{
    TeamList teamList;
    MatchManager o(&teamList);
    

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    MatchUnit::player_integral_map_type iv;

    MatchInfo info;
    info.set_roleid(teamMem.roleid());
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));

    o.Match(E_NOMAL_TEAM_MATCH, p);

    EXPECT_TRUE(o.InMatch(teamMem.roleid()));

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    EXPECT_FALSE(o.InMatch(teamMem.roleid()));


}


TEST(MatchMaking, JoinTeamInMatch_AgreeApply)
{
    TeamList teamList;
    MatchManager o(&teamList);
    

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    MatchUnit::player_integral_map_type iv;

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));


    teamMem.set_roleid(++nRoleId);

    o.GetTeamList().Apply(nTeamTypeTeamId, teamMem);

    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);

    
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));

    o.Match(E_NOMAL_TEAM_MATCH, p);
    EXPECT_FALSE(o.InMatch(teamMem.roleid()));
    o.GetTeamList().AgreeApplicant(nTeamTypeTeamId, teamMem.roleid());
    EXPECT_TRUE(o.InMatch(teamMem.roleid()));
}

TEST(MatchMaking, JoinTeamInMatch_AgreeInvite)
{
    TeamList teamList;
    MatchManager o(&teamList);
    

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    MatchUnit::player_integral_map_type iv;

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));


    teamMem.set_roleid(++nRoleId);

    o.GetTeamList().Invite(nTeamTypeTeamId, teamMem);

    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);

    MatchInfo info;
    info.set_roleid(teamMem.roleid());
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));

    o.Match(E_NOMAL_TEAM_MATCH, p);
    EXPECT_FALSE(o.InMatch(teamMem.roleid()));
    o.GetTeamList().AgreeInvite(nTeamTypeTeamId, teamMem.roleid());
    EXPECT_TRUE(o.InMatch(teamMem.roleid()));
}

TEST(MatchMaking, JoinTeamCancelMatch_AgreeInvite)
{
    TeamList teamList;
    MatchManager o(&teamList);
    

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    MatchUnit::player_integral_map_type iv;

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));


    teamMem.set_roleid(++nRoleId);

    o.GetTeamList().Invite(nTeamTypeTeamId, teamMem);

    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);

    MatchInfo info;
    info.set_roleid(teamMem.roleid());
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));

    o.Match(E_NOMAL_TEAM_MATCH, p);
    EXPECT_TRUE(o.InMatch(teamMem.roleid()));
    o.GetTeamList().AgreeInvite(nTeamTypeTeamId, teamMem.roleid());
    EXPECT_FALSE(o.InMatch(teamMem.roleid()));
}

TEST(MatchMaking, CreateCustomRoom)
{

    TeamList teamList;
    MatchManager o(&teamList);
    

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    MatchUnit::player_integral_map_type iv;

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    for (INT i = 1; i < 3; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
    }
    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_TEST_NOMAL_TEAM_RANK_MATCH, t, iv));
    for (int32_t i = 0; i < t.members_size(); ++i)
    {
        EXPECT_FALSE(o.InMatch(t.members(i).roleid()));
    }
    EXPECT_EQ(OR_MATCH_CUSTOM_ROOM_TYPE_ERRO, o.CreateCustomRoom(E_TEST_NOMAL_TEAM_RANK_MATCH, p));
    p.reset(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t, iv));

    for (int32_t i = 0; i < t.members_size(); ++i)
    {
        EXPECT_FALSE(o.InMatch(t.members(i).roleid()));
    }
    //EXPECT_EQ(OR_MATCH_CUSTOM_ROOM_TEAM_MEMBER_ERRO, o.CreateCustomRoom(E_NOMAL_TEAM_MATCH, p));
    for (INT i = 1; i < 3; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
    }
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    p.reset(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t, iv));
    EXPECT_EQ(OR_OK, o.CreateCustomRoom(E_NOMAL_TEAM_MATCH, p));
    for (int32_t i = 0; i < t.members_size(); ++i)
    {
        EXPECT_FALSE(o.InMatch(t.members(i).roleid()));
    }
    EXPECT_EQ(1, o.GetRoomSize(E_NOMAL_TEAM_MATCH));

    GUID_t roomRoleId = o.GetRoomId(t.members(0).roleid());
    for (int32_t i = 0; i < t.members_size(); ++i)
    {
        EXPECT_EQ(OR_OK, o.RoomReady(E_NOMAL_TEAM_MATCH, roomRoleId, t.members(i).roleid()));
    }
    for (int32_t i = 0; i < t.members_size(); ++i)
    {
        EXPECT_FALSE(o.InMatch(t.members(i).roleid()));
    }
    EXPECT_EQ(0, o.GetRoomSize(E_NOMAL_TEAM_MATCH));

}

TEST(MatchMaking, TeamLevelLimit)
{

    TeamList teamList;
    MatchManager o(&teamList);
    

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    GUID_t nLeaderRoleId = ++nRoleId;
    teamMem.set_roleid(nLeaderRoleId);

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    for (INT i = 1; i < 3; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
    }
    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));
    o.Match(E_NOMAL_TEAM_MATCH, p);
    for (int32_t i = 0; i < t.members_size(); ++i)
    {
        EXPECT_TRUE(o.InMatch(t.members(i).roleid()));
    }
    TeamTarget target;
    target.set_minlevel(2);
    target.set_maxlevel(10);
    teamList.SetTarget(nTeamTypeTeamId, nLeaderRoleId, target);
    MatchInfo info;
    info.set_roleid(++nRoleId);
    info.set_level(1);
    MatchMaking::match_unit_type p4(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
    
    o.Match(E_NOMAL_TEAM_MATCH, p);
    o.Match(E_NOMAL_TEAM_MATCH, p4);

    for (int32_t i = 0; i < t.members_size(); ++i)
    {
        EXPECT_TRUE(o.InMatch(t.members(i).roleid()));
    }
    EXPECT_TRUE(o.InMatch(info.roleid()));
}

TEST(MatchMaking, Match113Bug)
{

    TeamList teamList;
    MatchManager o(&teamList);
    

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;
    GUID_t nLeaderRoleId = ++nRoleId;
    teamMem.set_roleid(nLeaderRoleId);

    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    for (INT i = 0; i < 2; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
    }
    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));
    

    
    MatchInfo info;
    info.set_roleid(++nRoleId);
    MatchMaking::match_unit_type p4(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
    MatchInfo info1;
    info1.set_roleid(++nRoleId);
    MatchMaking::match_unit_type p5(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info1));
    
    o.Match(E_NOMAL_TEAM_MATCH, p4);
    o.Match(E_NOMAL_TEAM_MATCH, p5);
    o.Match(E_NOMAL_TEAM_MATCH, p);

    for (int32_t i = 0; i < t.members_size(); ++i)
    {
        EXPECT_FALSE(o.InMatch(t.members(i).roleid()));
    }
    EXPECT_FALSE(o.InMatch(info.roleid()));
    EXPECT_FALSE(o.InMatch(info1.roleid()));
}


TEST(MatchMaking, NormalOrder)
{
    TeamList teamList;
    MatchManager o(&teamList);
    
    MatchUnit::player_integral_map_type iv;
    GUID_t nRoleId = 1;

    for (int32_t jjj = 0; jjj < 100; ++jjj)
    {
        MatchInfo info;
        info.set_roleid(++nRoleId);
        if (jjj < 33)
        {
            iv[nRoleId].set_integral(1);
        }
        else
        {
            iv[nRoleId].set_integral(2);
        }

        MatchMaking::match_unit_type p4(new MatchUnit(0, E_MATCH_3V3, info, iv));
        EXPECT_EQ(OR_OK, o.Match(E_MATCH_3V3, p4));
    }
    std::cout << "room size : " << o.GetRoomSize(E_MATCH_3V3) << " remain player size : " << o.GetNotInPlayerRoomSize(E_MATCH_3V3) << std::endl;


    for (auto && it : o.GetMatchListForTest(E_MATCH_3V3, 0))
    {
       EXPECT_EQ(2, it.second->GetAvergeIntegral());
    }
}

TEST(MatchTest, MatchPlayerMatchGroupPlayer)
{
    TeamList teamList;
    MatchManager o(&teamList);
    
   
    MatchInfo info;
    GUID_t playerguid = 1;
    info.set_roleid(playerguid);
    MatchMaking::match_unit_type one(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, info));

    
   
 
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, one));

 
    EXPECT_EQ(1, one->GetPlayersSize());
    EXPECT_EQ(std::size_t(1), o.GetPlayerSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE));

    EXPECT_EQ(OR_MATCH_IN_MATCH_ERROR, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, one));
    EXPECT_EQ(OR_OK, o.CancelMatch(playerguid));
    EXPECT_FALSE(o.InMatch(1));
    EXPECT_EQ(0, o.GetPlayerSize());

    for (uint64_t i = 1; i < 5; ++i)
    {
        MatchInfo info;
        info.set_roleid(i);
        MatchMaking::match_unit_type test(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, info));
        EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, test));
        EXPECT_TRUE(o.InMatch(i));
        EXPECT_TRUE(o.InGroupMatch(i));
        EXPECT_EQ(OR_MATCH_IN_MATCH_ERROR, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, test));
        MatchMaking::room_type r = o.GetRoom(playerguid);
        EXPECT_EQ(NULL, r.get());
    }

    

   
    MatchMaking::room_type r =  o.ComeOutRestult(E_MATCH_MAX);

    EXPECT_EQ(NULL, r.get());
    r = o.ComeOutRestult(E_CROSS_SERVER_MATCH_DUNGEON_TYPE);

 

    playerguid = 105;
    info.set_roleid(playerguid);
    MatchMaking::match_unit_type testfive(new MatchUnit(1, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, info));

    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, testfive));
   
    EXPECT_TRUE(o.InMatch(playerguid));
    r = o.GetRoom(playerguid);
    EXPECT_EQ(NULL, r.get());

    playerguid = 105;
    info.set_roleid(playerguid);
    MatchMaking::match_unit_type testfive1(new MatchUnit(1, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, info));

  
    EXPECT_EQ(OR_MATCH_IN_MATCH_ERROR, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, testfive1));
    EXPECT_TRUE(o.InMatch(playerguid));
    r = o.GetRoom(playerguid);
    EXPECT_EQ(NULL, r.get());

    playerguid = 5;
    info.set_roleid(playerguid);
    MatchMaking::match_unit_type five(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, info));


    EXPECT_FALSE(o.InMatch(playerguid));


    for (uint64_t i = 1; i < 5; ++i)
    {
        EXPECT_TRUE(o.InMatch(i));
    }

    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, five));


    for (uint64_t i = 1; i < 6; ++i)
    {
        EXPECT_TRUE(o.InMatch(i));
    }

    r = o.GetRoom(playerguid);
    EXPECT_EQ(5, r->GetRoomPlayerSize());
    


    for (uint64_t i = 1; i < 6; ++i)
    {
        EXPECT_TRUE(o.InMatch(i));
    }

    o.OnEnterDungeon(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, r->GetRoomId());


    for (uint64_t i = 1; i < 6; ++i)
    {
        EXPECT_FALSE(o.InMatch(i));
        EXPECT_FALSE(o.InGroupMatch(i));
    }
}


TEST(MatchTest, MatchPlayerMatchExit)
{
    //team 
    TeamList teamList;
    MatchManager o(&teamList);
   
    TeamMemberInfo teamMem;
    teamMem.set_roleid(1);
    GUID_t leaderroleid = 1;
    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    GUID_t nRoleId = teamMem.roleid();
    for (INT i = 1; i < 5; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
    }

    
    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);


     
    
    MatchMaking::match_unit_type tp(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE,t));
    EXPECT_EQ(OR_MATCH_TEAM_CANT_MATCH__ERROR, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, tp));

    o.GetTeamList().LeaveTeam(nTeamTypeTeamId, nRoleId);

    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    MatchMaking::match_unit_type tpfour(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, t));
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, tpfour));


    EXPECT_EQ(OR_OK, o.CancelMatch(1));
    EXPECT_EQ(0, o.GetPlayerSize());
    EXPECT_EQ(0, o.GetPlayerSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE));
    for (GUID_t i = 1; i <5; ++i)
    {
        MatchInfo info;
        info.set_roleid(i);
        EXPECT_FALSE(o.InMatch(i));
    }

    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, tpfour));

    for (GUID_t i = 1; i < 5; ++i)
    {
        MatchInfo info;
        info.set_roleid(i);
        EXPECT_TRUE(o.InMatch(i));
    }
}


TEST(MatchTest, MatchPlayerMatchGroupPlayerReady)
{
    TeamList teamList;
    MatchManager o(&teamList);
   
    TeamMemberInfo teamMem;
    teamMem.set_roleid(1);
    GUID_t leaderroleid = 1;
    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    GUID_t nRoleId = teamMem.roleid();
    for (INT i = 1; i < 4; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
    }

    
    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);

    

    
    
    MatchMaking::match_unit_type tp(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, t));
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, tp));


    MatchInfo info;
    info.set_roleid(5);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, info));
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p));

    GUID_t roomId = o.GetRoomId(nRoleId);
    EXPECT_EQ(5, o.GetRoomPlayerSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, roomId));

    for (INT i = 1; i < 6; ++i)
    {
        EXPECT_EQ(OR_OK, o.RoomReady(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, roomId, i));
    }
    for (INT i = 1; i < 6; ++i)
    {
        EXPECT_FALSE(o.InMatch(i));
    }

}


TEST(MatchTest, MatchGroupPlayerCancle)
{
    TeamList teamList;
    MatchManager o(&teamList);
   
    TeamMemberInfo teamMem;
    teamMem.set_roleid(1);
    GUID_t leaderroleid = 1;
    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    GUID_t nRoleId = teamMem.roleid();
    for (INT i = 1; i < 4; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
    }

   
    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);

    
    MatchMaking::match_unit_type tp(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE,  t));
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, tp));


    MatchInfo info;

    info.set_roleid(++nRoleId);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, info));
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p));
    EXPECT_TRUE(o.InMatch(nRoleId));
    EXPECT_EQ(5, o.GetPlayerSize());
    MatchMaking::room_type r = o.GetRoom(nRoleId);
    EXPECT_EQ(5, r->GetRoomPlayerSize());

    for (INT i = 1; i < 5; ++i)
    {
        EXPECT_EQ(OR_OK, o.RoomReady(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, r->GetRoomId(), i));
    }
    o.RoomCancel(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, r->GetRoomId(), nRoleId);
    
    EXPECT_FALSE(o.InMatch(nRoleId));
    for (uint64_t i = 1; i < 5; ++i)
    {
        EXPECT_TRUE(o.InMatch(i));
    }

    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p));
    r = o.GetRoom(nRoleId);
    EXPECT_EQ(5, r->GetRoomPlayerSize());
    o.RoomCancel(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, r->GetRoomId(), 4);
    EXPECT_TRUE(o.InMatch(5));
    for (uint64_t i = 1; i < 5; ++i)
    {
        EXPECT_FALSE(o.InMatch(i));
    }

   

}

TEST(MatchMaking, MatchPlayerTeam24221)
{
    TeamList teamList;
    MatchManager o(&teamList);
    
    GUID_t nRoleId = 1;
   
    TeamMemberInfo teamMem;
    teamMem.set_roleid(nRoleId);
    Team::team_id_type nTeamTypeTeamId1 = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId1));
    for (INT i = 1; i < 2; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);
    }

    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, t));
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p));

    Team::team_id_type nTeamTypeTeamId2 = INVALID_ID;
    teamMem.set_roleid(++nRoleId);
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId2));
    for (INT i = 1; i < 4; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId2, teamMem);
    }


    o.GetTeamList().CopyTo(nTeamTypeTeamId2, t);
    MatchMaking::match_unit_type p1(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, t));
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p1));

    Team::team_id_type nTeamTypeTeamId3 = INVALID_ID;
    teamMem.set_roleid(++nRoleId);
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId3));
    for (INT i = 1; i < 2; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId3, teamMem);
    }


    o.GetTeamList().CopyTo(nTeamTypeTeamId3, t);
    MatchMaking::match_unit_type p2(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, t));
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p2));

    Team::team_id_type nTeamTypeTeamId4 = INVALID_ID;
    teamMem.set_roleid(++nRoleId);
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId4));
    for (INT i = 1; i < 2; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId4, teamMem);
    }

    o.GetTeamList().CopyTo(nTeamTypeTeamId4, t);
    MatchMaking::match_unit_type p3(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, t));
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p3));

    EXPECT_EQ(10, o.GetPlayerSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE));

    MatchInfo info;
    info.set_roleid(++nRoleId);
    MatchMaking::match_unit_type p4(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, info));
    EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p4));

    MatchMaking::room_type r = o.ComeOutRestult(E_CROSS_SERVER_MATCH_DUNGEON_TYPE);
    EXPECT_EQ(5, r->GetRoomPlayerSize());

    EXPECT_EQ(6, o.GetPlayerSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE));


    PBMatchRoom pb;

    r->ToClientPb(pb);

    for (int32_t i = 0; i < pb.camps_size(); ++i)
    {
        std::unordered_set<int32_t> campType;
        for (int32_t j = 0; j < pb.camps(i).players_size(); ++j)
        {
            campType.emplace(pb.camps(i).players(j).camp());
        }
        EXPECT_EQ(1, campType.size());
    }
}

TEST(MatchMaking, MatchPlayerTeamRandomPvp)
{
    TeamList teamList;
    MatchManager o(&teamList);
    
    GUID_t nRoleId = 1;
    typedef std::unordered_set<GUID_t> rolesets;
    rolesets vRoles;
   


    std::vector<Team::team_id_type> teamids;
    
    std::uniform_int_distribution<int32_t> dis(1, 3);
    for (int32_t jjj = 0; jjj < 1000; ++jjj)
    {
        if (std::rand() % 2 == 0)
        {
            TeamMemberInfo teamMem;
            teamMem.set_roleid(++nRoleId);
            vRoles.emplace(nRoleId);
            Team::team_id_type nTeamTypeTeamId1 = INVALID_ID;
            EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId1));

            int32_t maxi = dis(gen);
            for (INT i = 1; i < maxi; ++i)
            {
                teamMem.set_roleid(++nRoleId);
                vRoles.emplace(nRoleId);
                o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);
            }

            TeamInfo t;
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
            MatchMaking::match_unit_type p(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, t));
            EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p));

            teamids.push_back(nTeamTypeTeamId1);
        }
        else
        {
            MatchInfo info;
            info.set_roleid(++nRoleId);
            vRoles.emplace(nRoleId);
            MatchMaking::match_unit_type p4(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, info));
            EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p4));
        }

        std::size_t setremianplayersize = vRoles.size() - o.GetRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) * 5;
        std::size_t managersetremianplayersize = o.GetNotInPlayerRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE);
       
        EXPECT_EQ(setremianplayersize, managersetremianplayersize);
    }

    std::size_t matchsize = o.GetRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) * 5 + o.GetNotInPlayerRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE);
    EXPECT_EQ(matchsize, vRoles.size());
    EXPECT_EQ(o.GetPlayerSize(), vRoles.size());

    std::cout << "room size : " <<  o.GetRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) << "player size : " << o.GetPlayerSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE)  << std::endl;
    std::cout << " team size" << teamids.size() << std::endl;

    // test teammember in same room

    for (auto & it : teamids)
    {
        TeamInfo t;
        o.GetTeamList().CopyTo(it, t);
        std::unordered_set<uint64_t> roomset;
        for (int32_t i = 0; i < t.members_size(); ++i)
        {
            uint64_t roomId = o.GetRoomId(t.members(i).roleid());
            if (roomId > 0)
            {
                roomset.emplace(roomId);
            }
            if (!roomset.empty())
            {
                EXPECT_TRUE(roomset.find(roomId) != roomset.end());
            }
        }

        if (!roomset.empty())
        {
            EXPECT_EQ(1, roomset.size());
        }
        
    }

    std::size_t rolesize = vRoles.size();
    for (auto it : vRoles)
    {
        if (std::rand() % 3 == 0)
        {

            uint64_t roomId = o.GetRoomId(it);
            std::size_t cancelsize = 0;
            if (roomId > 0)
            {
                Team::team_id_type cteamid = o.GetTeamList().GetTeamId(it);
                if (cteamid > 0)
                {

                    TeamInfo t;
                    o.GetTeamList().CopyTo(cteamid, t);
                    cancelsize = t.members_size();
                }
                else
                {
                    cancelsize = 1;
                }

                std::size_t oldroomsize = o.GetRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE);
                EXPECT_EQ(OR_OK, o.RoomCancel(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, roomId, it));
                std::size_t newroomsize = o.GetRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE);

                rolesize -= cancelsize;

                EXPECT_EQ(rolesize, o.GetPlayerSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE));
                
                          
            }
            
        }

       

        std::size_t remainplayersize = rolesize - o.GetRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) * 5;
        std::size_t managersetremianplayersize = o.GetNotInPlayerRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE);

        EXPECT_EQ(remainplayersize, managersetremianplayersize);
    }

    matchsize = o.GetRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) * 5 + o.GetNotInPlayerRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE);
    EXPECT_EQ(matchsize, rolesize);
    EXPECT_EQ(matchsize, o.GetPlayerSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) );
    std::cout << "room size : " << o.GetRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) << "remain player size : " << o.GetNotInPlayerRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) << std::endl;
 
    for (auto & ri : o.GetRooms(E_CROSS_SERVER_MATCH_DUNGEON_TYPE))
    {
        PBMatchRoom pb;
        ri.second->ToClientPb(pb);
        for (int32_t i = 0; i < pb.camps_size(); ++i)
        {
            std::unordered_set<int32_t> campType;
            for (int32_t j = 0; j < pb.camps(i).players_size(); ++j)
            {
                campType.emplace(pb.camps(i).players(j).camp());
            }
            EXPECT_EQ(1, campType.size());
        }
    }
    
}



TEST(MatchMaking, MatchPlayer3V3)
{
   
    TeamList teamList;
    MatchManager o(&teamList);
    
    GUID_t nRoleId = 1;
    typedef std::unordered_set<GUID_t> rolesets;
    rolesets vRoles;
    std::vector<Team::team_id_type> teamids;
    std::uniform_int_distribution<int32_t> dis(1, 3);
    for (int32_t jjj = 0; jjj < 1000; ++jjj)
    {
        if (std::rand() % 2 == 0)
        {
            TeamMemberInfo teamMem;
            teamMem.set_roleid(++nRoleId);
            vRoles.emplace(nRoleId);
            Team::team_id_type nTeamTypeTeamId1 = INVALID_ID;
            EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId1));

            int32_t maxi = dis(gen);
            for (INT i = 1; i < maxi; ++i)
            {
                teamMem.set_roleid(++nRoleId);
                vRoles.emplace(nRoleId);
                o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);
            }

            TeamInfo t;
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
            MatchMaking::match_unit_type p(new MatchUnit(0, E_MATCH_3V3, t));
            int32_t ret = o.Match(E_MATCH_3V3, p);
            EXPECT_EQ(OR_OK, ret);

            teamids.push_back(nTeamTypeTeamId1);
        }
        else
        {
            MatchInfo info;
            info.set_roleid(++nRoleId);
            vRoles.emplace(nRoleId);
            MatchMaking::match_unit_type p4(new MatchUnit(0, E_MATCH_3V3, info));
            EXPECT_EQ(OR_OK, o.Match(E_MATCH_3V3, p4));
        }

        std::size_t setremianplayersize = vRoles.size() - o.GetRoomSize(E_MATCH_3V3) * 6;
        std::size_t managersetremianplayersize = o.GetNotInPlayerRoomSize(E_MATCH_3V3);

        EXPECT_EQ(setremianplayersize, managersetremianplayersize);
    }

    std::size_t matchsize = o.GetRoomSize(E_MATCH_3V3) * 6 + o.GetNotInPlayerRoomSize(E_MATCH_3V3);
    EXPECT_EQ(matchsize, vRoles.size());

    std::cout << "room size : " << o.GetRoomSize(E_MATCH_3V3) << "remain player size : " << o.GetNotInPlayerRoomSize(E_MATCH_3V3) << std::endl;
    std::cout << " team size" << teamids.size() << std::endl;


    for (auto & it : o.GetRooms(E_MATCH_3V3))
    {
        EXPECT_EQ(3 * 2, it.second->GetRoomPlayerSize());
        PBMatchRoom pb;
        it.second->ToClientPb(pb);
        for (int32_t i = 0; i < pb.camps_size(); ++i)
        {
            std::unordered_set<int32_t> campType;
            for (int32_t j = 0; j < pb.camps(i).players_size(); ++j)
            {
                Team::team_id_type teamid = o.GetTeamList().GetTeamId(pb.camps(i).players(j).roleid());
       
                if (teamid > 0)
                {
                    
                    TeamInfo ti;
                    o.GetTeamList().CopyTo(teamid, ti);
                    EXPECT_TRUE(it.second->TestTeamInTheSameCamp(ti));
                }
                campType.emplace(pb.camps(i).players(j).camp());
            }
            EXPECT_EQ(1, campType.size());

        }
    }

}

TEST(MatchMaking, MatchPlayer15V15)
{
    TeamList teamList;
    MatchManager o(&teamList);
    
    GUID_t nRoleId = 1;
    typedef std::unordered_set<GUID_t> rolesets;
    rolesets vRoles;
   


    std::vector<Team::team_id_type> teamids;

    std::uniform_int_distribution<int32_t> dis(1, 5);
    for (int32_t jjj = 0; jjj < 1000; ++jjj)
    {
        if (std::rand() % 2 == 0)
        {
            TeamMemberInfo teamMem;
            teamMem.set_roleid(++nRoleId);
            vRoles.emplace(nRoleId);
            Team::team_id_type nTeamTypeTeamId1 = INVALID_ID;
            EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId1));

            int32_t maxi = dis(gen);
            for (INT i = 1; i < maxi; ++i)
            {
                teamMem.set_roleid(++nRoleId);
                vRoles.emplace(nRoleId);
                o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);
            }

            TeamInfo t;
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
            MatchMaking::match_unit_type p(new MatchUnit(0, E_MATCH_15V15, t));
            EXPECT_EQ(OR_OK, o.Match(E_MATCH_15V15, p));

            teamids.push_back(nTeamTypeTeamId1);
        }
        else
        {
            MatchInfo info;
            info.set_roleid(++nRoleId);
            vRoles.emplace(nRoleId);
            MatchMaking::match_unit_type p4(new MatchUnit(0, E_MATCH_15V15, info));
            EXPECT_EQ(OR_OK, o.Match(E_MATCH_15V15, p4));
        }

        std::size_t setremianplayersize = vRoles.size() - o.GetRoomSize(E_MATCH_15V15) * 15 * 2;
        std::size_t managersetremianplayersize = o.GetNotInPlayerRoomSize(E_MATCH_15V15);

        EXPECT_EQ(setremianplayersize, managersetremianplayersize);
    }

    std::size_t matchsize = o.GetRoomSize(E_MATCH_15V15) * 15 * 2 + o.GetNotInPlayerRoomSize(E_MATCH_15V15);
    EXPECT_EQ(matchsize, vRoles.size());

    std::cout << "room size : " << o.GetRoomSize(E_MATCH_15V15) << "remain player size : " << o.GetNotInPlayerRoomSize(E_MATCH_15V15) << std::endl;
    std::cout << " team size" << teamids.size() << std::endl;


    for (auto it : o.GetRooms(E_MATCH_15V15))
    {
        EXPECT_EQ(15 * 2, it.second->GetRoomPlayerSize());
        PBMatchRoom pb;
        it.second->ToClientPb(pb);
        for (int32_t i = 0; i < pb.camps_size(); ++i)
        {
            std::unordered_set<int32_t> campType;
            for (int32_t j = 0; j < pb.camps(i).players_size(); ++j)
            {
                Team::team_id_type teamid = o.GetTeamList().GetTeamId(pb.camps(i).players(j).roleid());

                if (teamid > 0)
                {

                    TeamInfo ti;
                    o.GetTeamList().CopyTo(teamid, ti);
                    EXPECT_TRUE(it.second->TestTeamInTheSameCamp(ti));
                }
                campType.emplace(pb.camps(i).players(j).camp());
            }
            EXPECT_EQ(1, campType.size());

        }
    }
}


TEST(MatchMaking, MatchMatchPlayerCancelExitProfession)
{
    TeamList teamList;
    MatchManager o(&teamList);
    o.SetWithoutRepetitionProfession(true);
   
    GUID_t nRoleId = 1;
    typedef std::unordered_set<GUID_t> rolesets;
    typedef std::vector<GUID_t> rolevec;
    rolevec vecRoles;


    rolesets vRoles;
   


    std::vector<Team::team_id_type> teamids;

    std::uniform_int_distribution<int32_t> dis(1, 3);
    std::uniform_int_distribution<int32_t> pdis(1, 3);
    std::size_t rolesize = vRoles.size();

    rolevec exitrole;


    for (int32_t jjj = 0; jjj < 1000; ++jjj)
    {
        if (std::rand() % 2 == 0)
        {
            TeamMemberInfo teamMem;
            teamMem.set_roleid(++nRoleId);
            teamMem.set_professionid(pdis(gen));
            vRoles.emplace(nRoleId);
            vecRoles.push_back(nRoleId);
            Team::team_id_type nTeamTypeTeamId1 = INVALID_ID;
            EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId1));

            int32_t maxi = dis(gen);
            for (INT i = 1; i < maxi; ++i)
            {
                teamMem.set_roleid(++nRoleId);
                teamMem.set_professionid(pdis(gen));
                vRoles.emplace(nRoleId);
                vecRoles.push_back(nRoleId);
                o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);
            }

            TeamInfo t;
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
            MatchMaking::match_unit_type p(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, t));
            EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p));

            teamids.push_back(nTeamTypeTeamId1);
        }
        else
        {
            MatchInfo info;
            info.set_roleid(++nRoleId);
            info.set_professionid(pdis(gen));
            vRoles.emplace(nRoleId);
            vecRoles.push_back(nRoleId);
            MatchMaking::match_unit_type p4(new MatchUnit(0, E_CROSS_SERVER_MATCH_DUNGEON_TYPE, info));
            EXPECT_EQ(OR_OK, o.Match(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, p4));
        }



        if (std::rand() % 3 == 0 && !vecRoles.empty())
        {
            std::uniform_int_distribution<int32_t> dis(0, vecRoles.size() - 1);

            int32_t rpos = dis(gen);
            uint64_t roomId = o.GetRoomId(vecRoles[rpos]);
            std::size_t cancelsize = 0;
            if (roomId > 0)
            {
                Team::team_id_type cteamid = o.GetTeamList().GetTeamId(vecRoles[rpos]);
                if (cteamid > 0)
                {

                    TeamInfo t;
                    o.GetTeamList().CopyTo(cteamid, t);
                    cancelsize = t.members_size();

                    for (int32_t k = 0; k < t.members_size(); ++k)
                    {
                        exitrole.push_back(t.members(k).roleid());
                    }
                }
                else
                {
                    cancelsize = 1;
                    exitrole.push_back(vecRoles[rpos]);
                }

                o.RoomCancel(E_CROSS_SERVER_MATCH_DUNGEON_TYPE, roomId, vecRoles[rpos]);
                rolesize -= cancelsize;
            }

        }
        if (std::rand() % 2 == 1 && !vecRoles.empty())
        {


            std::uniform_int_distribution<int32_t> dis(0, vecRoles.size() - 1);

            int32_t rpos = dis(gen);

            std::size_t cancelsize = 0;
            if (o.InMatch(vecRoles[rpos]))
            {

                Team::team_id_type cteamid = o.GetTeamList().GetTeamId(vecRoles[rpos]);
                if (cteamid > 0)
                {

                    TeamInfo t;
                    o.GetTeamList().CopyTo(cteamid, t);
                    cancelsize = t.members_size();

                    for (int32_t k = 0; k < t.members_size(); ++k)
                    {
                        exitrole.push_back(t.members(k).roleid());
                    }
                }
                else
                {
                    cancelsize = 1;
                    exitrole.push_back(vecRoles[rpos]);
                }

                o.CancelMatch(vecRoles[rpos]);
            }

        }
    }

    std::size_t matchsize = o.GetRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) * 5 + o.GetNotInPlayerRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE);
    EXPECT_EQ(matchsize, vRoles.size() - exitrole.size());
    std::cout << "room size : " << o.GetRoomSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) << "player size : " << o.GetPlayerSize(E_CROSS_SERVER_MATCH_DUNGEON_TYPE) << std::endl;
    std::cout << " team size" << teamids.size() << std::endl;
    int32_t t = 0;
    for (auto it : o.GetRooms(E_CROSS_SERVER_MATCH_DUNGEON_TYPE))
    {

        EXPECT_EQ(5, it.second->GetRoomPlayerSize());
        if (it.second->GetMatchUnit().size() >= 3)
        {
            EXPECT_EQ(3, it.second->GetPrefession().size());
            ++t;
        }

        PBMatchRoom pb;
        it.second->ToClientPb(pb);
        for (int32_t i = 0; i < pb.camps_size(); ++i)
        {
            std::unordered_set<int32_t> campType;
            for (int32_t j = 0; j < pb.camps(i).players_size(); ++j)
            {
                Team::team_id_type teamid = o.GetTeamList().GetTeamId(pb.camps(i).players(j).roleid());

                if (teamid > 0)
                {

                    TeamInfo ti;
                    o.GetTeamList().CopyTo(teamid, ti);
                    EXPECT_TRUE(it.second->TestTeamInTheSameCamp(ti));
                }
                campType.emplace(pb.camps(i).players(j).camp());
            }
            EXPECT_EQ(1, campType.size());

        }
       
    }


}

TEST(MatchMaking, NormalTeamMatch)
{
    TeamList teamList;
    MatchManager o(&teamList);
    o.SetWithoutRepetitionProfession(true);
    GUID_t nRoleId = 1;
    typedef std::unordered_set<GUID_t> rolesets;
    typedef std::vector<GUID_t> rolevec;
    rolevec vecRoles;


    rolesets vRoles;
   


    std::vector<Team::team_id_type> teamids;

    std::uniform_int_distribution<int32_t> dis(1, 3);
    std::uniform_int_distribution<int32_t> pdis(1, 3);
    std::size_t rolesize = vRoles.size();

    rolevec exitrole;
    int32_t maxi = 1;
    for (int32_t jjj = 0; jjj < 4; ++jjj)
    {

            TeamMemberInfo teamMem;
            teamMem.set_roleid(++nRoleId);
            teamMem.set_professionid(pdis(gen));
            vRoles.emplace(nRoleId);
            vecRoles.push_back(nRoleId);
            Team::team_id_type nTeamTypeTeamId1 = INVALID_ID;
            EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId1));

            
            for (INT i = 1; i < maxi; ++i)
            {
                teamMem.set_roleid(++nRoleId);
                teamMem.set_professionid(pdis(gen));
                vRoles.emplace(nRoleId);
                vecRoles.push_back(nRoleId);
                o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);
            }

            TeamInfo t;
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
            MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));
            EXPECT_EQ(OR_OK, o.Match(E_NOMAL_TEAM_MATCH, p));

            teamids.push_back(nTeamTypeTeamId1);
            ++maxi;
      
    }

    for (auto it : teamids)
    {

        TeamInfo t;
        o.GetTeamList().CopyTo(it, t);
        int32_t s = 5 - t.members_size();
        for (int32_t m = 0; m < s; ++m)
        {
            MatchInfo info;
            info.set_roleid(++nRoleId);
            vRoles.emplace(nRoleId);
            vecRoles.push_back(nRoleId);
            MatchMaking::match_unit_type p4(new MatchUnit(0, E_NOMAL_TEAM_MATCH, info));
            EXPECT_EQ(OR_OK, o.Match(E_NOMAL_TEAM_MATCH, p4));
        }
    }
    for (auto it : teamids)
    {
        TeamInfo t;
        o.GetTeamList().CopyTo(it, t);
        EXPECT_EQ(5, t.members_size());
       
    }

   

    EXPECT_EQ(0, o.GetRoomSize(E_NOMAL_TEAM_MATCH));
    EXPECT_EQ(0, o.GetPlayerSize(E_NOMAL_TEAM_MATCH));
    EXPECT_EQ(0, o.GetAllMatchUnitSize(E_NOMAL_TEAM_MATCH));

    vecRoles.clear();
    teamids.clear();
    for (int32_t jjj = 0; jjj < 1000; ++jjj)
    {
        
            TeamMemberInfo teamMem;
            teamMem.set_roleid(++nRoleId);
            teamMem.set_professionid(pdis(gen));
            vRoles.emplace(nRoleId);
            vecRoles.push_back(nRoleId);
            Team::team_id_type nTeamTypeTeamId1 = INVALID_ID;
            EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId1));

            int32_t maxi = dis(gen);
            for (INT i = 1; i < maxi; ++i)
            {
                teamMem.set_roleid(++nRoleId);
                teamMem.set_professionid(pdis(gen));
                vRoles.emplace(nRoleId);
                vecRoles.push_back(nRoleId);
                o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);
            }
            TeamInfo t;
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
            MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH,  t));
            EXPECT_EQ(OR_OK, o.Match(E_NOMAL_TEAM_MATCH, p));

            teamids.push_back(nTeamTypeTeamId1);
    }

    EXPECT_EQ(vecRoles.size(), o.GetPlayerSize(E_NOMAL_TEAM_MATCH));
    EXPECT_EQ(teamids.size(), o.GetAllMatchUnitSize(E_NOMAL_TEAM_MATCH));

   
    for (auto it : teamids)
    {
        Team::team_id_type nTeamTypeTeamId1 = it;
        TeamInfo t;
        o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
        if (t.members_size() <= 1)
        {
            continue;
        }
   
        std::uniform_int_distribution<int32_t> tis(0, t.members_size() - 1);
        int32_t ri = tis(gen);
        GUID_t roleId = t.members(ri).roleid();
        if (o.GetTeamList().IsLeader(nTeamTypeTeamId1, roleId))
        {
            TeamInfo t;
            o.GetTeamList().LeaveTeam(nTeamTypeTeamId1, roleId);
            EXPECT_FALSE(o.InMatch(roleId));
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);

            for (int32_t i = 0; i < t.members_size(); ++i)
            {
                EXPECT_FALSE(o.InMatch(t.members(i).roleid()));
            }

        }
        else
        {
            int32_t p = 0;
            TeamInfo t;
            o.GetTeamList().LeaveTeam(nTeamTypeTeamId1, roleId);
            TeamMemberInfo ts;
            ts.set_roleid(roleId);
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, ts);
            EXPECT_FALSE(o.InMatch(roleId));
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
            bool test = false;
            for (int32_t i = 0; i < t.members_size(); ++i)
            {
                bool ret = o.InMatch(t.members(i).roleid());
                EXPECT_TRUE(o.InMatch(t.members(i).roleid()));

            }

        }

    }

}


TEST(MatchMaking, MatchPlayerMatchCancelTimeOutExit)
{
    TeamList teamList;
    MatchManager o(&teamList);
    
    GUID_t nRoleId = 1;
    typedef std::unordered_set<GUID_t> rolesets;
    typedef std::vector<GUID_t> rolevec;
    rolevec vecRoles;


    rolesets vRoles;

    rolesets teamGuids;

    std::vector<Team::team_id_type> teamids;

    std::uniform_int_distribution<int32_t> dis(1, 3);
    std::size_t rolesize = vRoles.size();

    rolevec exitrole;


    for (int32_t jjj = 0; jjj < 1000; ++jjj)
    {
        if (std::rand() % 2 == 0)
        {
            TeamMemberInfo teamMem;
            teamMem.set_roleid(++nRoleId);
            vRoles.emplace(nRoleId);
            vecRoles.push_back(nRoleId);
            teamGuids.emplace(nRoleId);
            Team::team_id_type nTeamTypeTeamId1 = INVALID_ID;
            EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId1));

            int32_t maxi = dis(gen);
            for (INT i = 1; i < maxi; ++i)
            {
                teamMem.set_roleid(++nRoleId);
                teamGuids.emplace(nRoleId);
                vRoles.emplace(nRoleId);
                vecRoles.push_back(nRoleId);
                o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);
            }

            TeamInfo t;
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
            MatchMaking::match_unit_type p(new MatchUnit(0, E_MATCH_15V15, t));
            EXPECT_EQ(OR_OK, o.Match(E_MATCH_15V15, p));

            teamids.push_back(nTeamTypeTeamId1);
        }
        else
        {
            MatchInfo info;
            info.set_roleid(++nRoleId);
            vRoles.emplace(nRoleId);
            vecRoles.push_back(nRoleId);
            MatchMaking::match_unit_type p4(new MatchUnit(0, E_MATCH_15V15, info));
            EXPECT_EQ(OR_OK, o.Match(E_MATCH_15V15, p4));
        }

    }

    std::size_t matchsize = o.GetPlayerSize(E_MATCH_15V15);
    EXPECT_EQ(matchsize, vRoles.size() - exitrole.size());

    std::cout << "room size : " << o.GetRoomSize(E_MATCH_15V15) << "remain player size : " << o.GetNotInPlayerRoomSize(E_MATCH_15V15) << std::endl;

    rolevec roomexitrole;

    MatchMaking::roome_list_type rs = o.GetRooms(E_MATCH_15V15);
    for (auto  & ri : rs)
    {
        ri.second->SetAllReady();

        GUID_t g = ri.second->RandomGuid();

        ri.second->SetNone(g);
        
        MatchRoom::room_guids_type gs;

        Team::team_id_type tid = teamList.GetTeamId(g);
        if (tid > Team::EmptyTeamId)
        {
            gs = ri.second->GetTeamGuid(g);
            TeamInfo t;
            o.GetTeamList().CopyTo(tid, t);
            for (int32_t i = 0; i < t.members_size(); ++i)
            {
                GUID_t rid = t.members(i).roleid();
                roomexitrole.push_back(rid);
                vRoles.erase(rid);
                EXPECT_TRUE(gs.find(rid) != gs.end());
            }

            

        }
        else
        {
            gs = ri.second->GetTeamGuid(g);

         
                roomexitrole.push_back(g);
                vRoles.erase(g);
     
        }
        ri.second->TimeOut();
        for (auto gi : gs)
        {
            EXPECT_FALSE(o.InMatch(gi));
        }
    }


    for (auto it : roomexitrole)
    {
        EXPECT_FALSE(o.InMatch(it));
    }

    for (auto it : vRoles)
    {
        EXPECT_TRUE(o.InMatch(it));
    }
}

TEST(MatchMaking, AppointLeaderCancelMatch)
{
    TeamList teamList;
    MatchManager o(&teamList);
    
    GUID_t nRoleId = 1;
 
    typedef std::vector<GUID_t> rolevec;
    rolevec roleVec;

    TeamMemberInfo teamMem;
    teamMem.set_roleid(++nRoleId);
    roleVec.push_back(nRoleId);
    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    for (INT i = 1; i < 4; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        roleVec.push_back(nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
    }

    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_NOMAL_TEAM_MATCH, t));
    EXPECT_EQ(OR_OK, o.Match(E_NOMAL_TEAM_MATCH, p));
    o.GetTeamList().AppointLeader(nTeamTypeTeamId, 2, nRoleId);

    for (auto && roleIt : roleVec)
    {
        EXPECT_FALSE(o.InMatch(roleIt));
    }
}

TEST(MatchMaking, MatchSection)
{
    TeamList teamList;
    MatchManager o(&teamList);
    

    typedef std::vector<GUID_t> rolevec;
    rolevec roleVec;

    GUID_t nRoleId = 1;
    TeamMemberInfo teamMem;

    teamMem.set_roleid(++nRoleId);
    roleVec.push_back(nRoleId);
    MatchUnit::player_integral_map_type iv;
    int32_t ns = 1;
    iv[nRoleId].set_section(ns);
    Team::team_id_type nTeamTypeTeamId = INVALID_ID;
    EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));

    for (INT i = 1; i < 4; ++i)
    {
        teamMem.set_roleid(++nRoleId);
        roleVec.push_back(nRoleId);
        o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);
    }

    iv[2].set_section(1);
    iv[3].set_section(2);
    iv[4].set_section(3);
    iv[5].set_section(1);


    MatchInfo info;
    info.set_roleid(++nRoleId);
    roleVec.push_back(nRoleId);
    info.set_section(4);
    MatchMaking::match_unit_type p4(new MatchUnit(0, E_TEST_NOMAL_TEAM_RANK_MATCH, info));

    TeamInfo t;
    o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
    MatchMaking::match_unit_type p(new MatchUnit(0, E_TEST_NOMAL_TEAM_RANK_MATCH, t, iv));
    EXPECT_EQ(OR_MATCH_TEAM_SECTION_ERROR, o.Match(E_TEST_NOMAL_TEAM_RANK_MATCH, p));
    iv[4].set_section(2);
    EXPECT_EQ(OR_OK, o.Match(E_TEST_NOMAL_TEAM_RANK_MATCH, p4));
    p.reset(new MatchUnit(0, E_TEST_NOMAL_TEAM_RANK_MATCH, t, iv));
    EXPECT_EQ(OR_OK, o.Match(E_TEST_NOMAL_TEAM_RANK_MATCH, p));
    for (auto && rit : roleVec)
    {
        EXPECT_TRUE(o.InMatch(rit));
    }

    EXPECT_EQ(OR_OK, o.CancelMatch(nRoleId));
    info.set_section(1);
    p4.reset(new MatchUnit(0, E_TEST_NOMAL_TEAM_RANK_MATCH, info));
    EXPECT_EQ(OR_OK, o.Match(E_TEST_NOMAL_TEAM_RANK_MATCH, p4));

    MatchMaking::room_type r = o.ComeOutRestult(E_TEST_NOMAL_TEAM_RANK_MATCH);
    EXPECT_TRUE(NULL != r);

    for (auto && rit : roleVec)
    {
        EXPECT_FALSE(o.InMatch(rit));
    }
}

TEST(MatchMaking, MatchRandomSection)
{
    TeamList teamList;
    MatchManager o(&teamList);
    
    

    GUID_t nRoleId = 1;

    const MatchElement* pMatchElement = MatchTable::Instance().GetElement(E_TEST_NOMAL_TEAM_RANK_MATCH);
    std::uniform_int_distribution<int32_t> dis(pMatchElement->sections_list[0], pMatchElement->sections_list[pMatchElement->sections_list.size() - 1]);
    

    for (int32_t i = 0; i < 1000; ++i)
    {
        Team::team_id_type nTeamTypeTeamId = INVALID_ID;
        
        MatchUnit::player_integral_map_type iv;
        TeamMemberInfo teamMem;
        teamMem.set_roleid(++nRoleId);
        EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId));
        iv[nRoleId].set_section(dis(gen));

        int32_t minSection = iv[nRoleId].section();
        int32_t maxSection = iv[nRoleId].section();
        for (int32_t j = 1; i < 4; ++i)
        {
            teamMem.set_roleid(++nRoleId);
            iv[nRoleId].set_section(dis(gen));
            o.GetTeamList().JoinTeam(nTeamTypeTeamId, teamMem);

            minSection = std::min(iv[nRoleId].section(), minSection);
            maxSection = std::max(iv[nRoleId].section(), maxSection);
        }
        TeamInfo t;
        o.GetTeamList().CopyTo(nTeamTypeTeamId, t);
        MatchMaking::match_unit_type p(new MatchUnit(0, E_TEST_NOMAL_TEAM_RANK_MATCH, t, iv));

        int32_t ret = o.Match(E_TEST_NOMAL_TEAM_RANK_MATCH, p);
        if (maxSection - minSection > pMatchElement->team_section_span)
        {
            EXPECT_EQ(OR_MATCH_TEAM_SECTION_ERROR, ret);
        }
        else
        {
            EXPECT_EQ(OR_OK, ret);
        }
        
    }

    std::size_t matchsize = o.GetRoomSize(E_TEST_NOMAL_TEAM_RANK_MATCH) * 15 * 2 + o.GetNotInPlayerRoomSize(E_TEST_NOMAL_TEAM_RANK_MATCH);
    std::cout << "room size : " << o.GetRoomSize(E_TEST_NOMAL_TEAM_RANK_MATCH) << " remain player size : " << o.GetNotInPlayerRoomSize(E_TEST_NOMAL_TEAM_RANK_MATCH) << std::endl;
}

TEST(MatchMaking, MatchRandomSectionProfession)
{
    TeamList teamList;
    MatchManager o(&teamList);
    o.SetWithoutRepetitionProfession(true);

    GUID_t nRoleId = 1;
    typedef std::unordered_set<GUID_t> rolesets;
    typedef std::vector<GUID_t> rolevec;
    rolevec vecRoles;
    rolesets vRoles;
    std::vector<Team::team_id_type> teamids;

    std::uniform_int_distribution<int32_t> prefessiondis(1, 3);
    std::size_t rolesize = vRoles.size();
    const MatchElement* pMatchElement = MatchTable::Instance().GetElement(E_MATCH_15V15);
    std::uniform_int_distribution<int32_t> sectiondis(pMatchElement->sections_list[0], pMatchElement->sections_list[pMatchElement->sections_list.size() - 1]);
    rolevec exitrole;


    for (int32_t jjj = 0; jjj < 1000; ++jjj)
    {
        MatchUnit::player_integral_map_type iv;
        if (std::rand() % 2 == 0)
        {
            TeamMemberInfo teamMem;
            teamMem.set_roleid(++nRoleId);
            teamMem.set_professionid(prefessiondis(gen));
            vRoles.emplace(nRoleId);
            iv[nRoleId].set_section(sectiondis(gen));
            vecRoles.push_back(nRoleId);
            Team::team_id_type nTeamTypeTeamId1 = INVALID_ID;
            EXPECT_EQ(OR_OK, o.GetTeamList().CreateTeam(teamMem, Team::E_TEAM_TYPE, nTeamTypeTeamId1));
            int32_t minSection = iv[nRoleId].section();
            int32_t maxSection = iv[nRoleId].section();
            int32_t maxi = 4;
            for (INT i = 1; i < maxi; ++i)
            {
                teamMem.set_roleid(++nRoleId);
                iv[nRoleId].set_section(sectiondis(gen));
                teamMem.set_professionid(prefessiondis(gen));
                vRoles.emplace(nRoleId);
                vecRoles.push_back(nRoleId);
                o.GetTeamList().JoinTeam(nTeamTypeTeamId1, teamMem);
                minSection = std::min(iv[nRoleId].section(), minSection);
                maxSection = std::max(iv[nRoleId].section(), maxSection);
            }

            TeamInfo t;
            o.GetTeamList().CopyTo(nTeamTypeTeamId1, t);
            MatchMaking::match_unit_type p(new MatchUnit(0, E_MATCH_15V15, t, iv));

            int32_t ret = o.Match(E_MATCH_15V15, p);
            if (maxSection - minSection > pMatchElement->team_section_span)
            {
                EXPECT_EQ(OR_MATCH_TEAM_SECTION_ERROR, ret);

                for (auto && ci : iv)
                {
                    exitrole.push_back(ci.first);
                }
            }
            else
            {
                EXPECT_EQ(OR_OK, ret);
            }

            teamids.push_back(nTeamTypeTeamId1);
        }
        else
        {
            MatchInfo info;
            info.set_roleid(++nRoleId);
            info.set_professionid(prefessiondis(gen));
            vRoles.emplace(nRoleId);
            vecRoles.push_back(nRoleId);
            iv[nRoleId].set_section(sectiondis(gen));
            MatchMaking::match_unit_type p4(new MatchUnit(0, E_MATCH_15V15, info, iv));
            EXPECT_EQ(OR_OK, o.Match(E_MATCH_15V15, p4));
        }

    }


    std::size_t matchsize = o.GetRoomSize(E_MATCH_15V15) * 15 * 2 + o.GetNotInPlayerRoomSize(E_MATCH_15V15);
    EXPECT_EQ(matchsize, vRoles.size() - exitrole.size());
    std::cout << "room size : " << o.GetRoomSize(E_MATCH_15V15) << " remain player size : " << o.GetNotInPlayerRoomSize(E_MATCH_15V15) << std::endl;
    std::cout << " team size" << teamids.size() << std::endl;
    int32_t t = 0;
    for (auto it : o.GetRooms(E_MATCH_15V15))
    {

        EXPECT_EQ(30, it.second->GetRoomPlayerSize());
        if (it.second->GetMatchUnit().size() >= std::size_t(pMatchElement->profession_size))
        {
            EXPECT_EQ(pMatchElement->profession_size, it.second->GetPrefession().size());
            ++t;
        }

        PBMatchRoom pb;
        it.second->ToClientPb(pb);
        for (int32_t i = 0; i < pb.camps_size(); ++i)
        {
            std::unordered_set<int32_t> campType;
            for (int32_t j = 0; j < pb.camps(i).players_size(); ++j)
            {
                Team::team_id_type teamid = o.GetTeamList().GetTeamId(pb.camps(i).players(j).roleid());

                if (teamid > 0)
                {

                    TeamInfo ti;
                    o.GetTeamList().CopyTo(teamid, ti);
                    EXPECT_TRUE(it.second->TestTeamInTheSameCamp(ti));
                }
                campType.emplace(pb.camps(i).players(j).camp());
            }
            EXPECT_EQ(1, campType.size());

        }
        
        {
            int32_t minSection = 0;
            int32_t maxSection = 0;
            int32_t testIndex = 0;
            for (auto && pi : it.second->GetPlayers())
            {
                if (testIndex == 0)
                {

                    minSection = pi.second.section();
                    maxSection = pi.second.section();
                    ++testIndex;
                    continue;
                }
                minSection = std::min(pi.second.section(), minSection);
                maxSection = std::max(pi.second.section(), maxSection);
                ++testIndex;
            }
            bool bTestRet = maxSection - minSection <= pMatchElement->team_section_span;
            EXPECT_TRUE(bTestRet);
        }
    }
}




int main(int argc, char **argv)
{
    muduo::net::EventLoop loop;
    pLoop = &loop;
    BaseModule::SetThreadLocalStorageLoop(pLoop);

    MatchTable::Instance().Load();
	testing::InitGoogleTest(&argc, argv);

    for (int32_t i = 0; i < 10; ++i)
    {
        RUN_ALL_TESTS();
    }
	return RUN_ALL_TESTS();
}
