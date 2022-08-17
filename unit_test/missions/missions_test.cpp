#include <gtest/gtest.h>

#include "src/game_config/condition_config.h"
#include "src/game_config/mission_config.h"
#include "src/game_logic/comp/mission_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/op_code.h"
#include "src/game_logic/missions/missions_base.h"
#include "src/game_logic/missions/player_mission.h"
#include "src/util/random.h"
#include "src/game_logic/tips_id.h"

#include "comp.pb.h"

TEST(MissionsComp, AcceptMission)
{
    uint32_t mid = 1;
    MissionsComp ms;
    registry.remove<CheckSubType>(ms);
    AcceptMissionP param{mid};
    auto& data = mission_config::GetSingleton().all();
    std::size_t sz = 0;
    for (int32_t i = 0; i < data.data_size(); ++i)
    {
        auto id = data.data(i).id();
        param.mission_id_ = id;
        auto m = ms.Accept(param);
        ++sz;
    }

    EXPECT_EQ(sz, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());
    ms.CompleteAllMission();
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(sz, ms.complete_size());
       
}

TEST(MissionsComp, RepeatedMission)
{
    MissionsComp ms;
    {
        uint32_t mid = 1;
        AcceptMissionP param{mid};
        EXPECT_EQ(kRetOK, ms.Accept(param));
        EXPECT_EQ(kRetMissionIdRepeated, ms.Accept(param));
    }

    {
        AcceptMissionP param{ 3 };
		AcceptMissionP param2{ 2 };
		EXPECT_EQ(kRetOK, ms.Accept(param));
		EXPECT_EQ(kRetMisionTypeRepeated, ms.Accept(param2));
    }
}

TEST(MissionsComp, TriggerCondition)
{
    MissionsComp ms;
    uint32_t mid = 1;
    //auto mrow = mission_config::GetSingleton().get(mid);
    AcceptMissionP param{mid};
    EXPECT_EQ(kRetOK, ms.Accept(param));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{ E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.condtion_ids_ = { 2 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.condtion_ids_ = { 3 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.condtion_ids_ = { 4 };
    ms.receive(ce);
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(1, ms.complete_size());
    EXPECT_EQ(0, ms.type_set_size());
}

TEST(MissionsComp, TypeSize)
{
    MissionsComp ms;
    uint32_t mid = 6;
    //auto mrow = mission_config::GetSingleton().get(mid);
    AcceptMissionP param{ mid };
    EXPECT_EQ(kRetOK, ms.Accept(param));
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_COMSTUM; ++i)
    {
        EXPECT_EQ(1, ms.classify_for_unittest().find(i)->second.size());
    }    
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.type_ = E_CONDITION_TALK_WITH_NPC;
    ce.condtion_ids_ = { 1 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.type_ = E_CONDITION_COMPLELETE_CONDITION;
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.type_ = E_CONDITION_USE_ITEM;
    ce.condtion_ids_ = { 1, 2 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.type_ = E_CONDITION_LEVEUP;
    ce.condtion_ids_ = { 10 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.type_ = E_CONDITION_INTERATION;
    ce.condtion_ids_ = { 1, 2};
    ms.receive(ce);
    
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(1, ms.complete_size());
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_EQ(0, ms.type_set_size());
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_COMSTUM; ++i)
    {
        EXPECT_EQ(0, ms.classify_for_unittest().find(i)->second.size());
    }
}

TEST(MissionsComp, CompleteAcceptMission)
{
    MissionsComp ms;
    uint32_t mid = 4;
    //auto mrow = mission_config::GetSingleton().get(mid);
    AcceptMissionP param{ mid };
    EXPECT_EQ(kRetOK, ms.Accept(param));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_EQ(kRetMissionComplete, ms.Accept(param));
}

TEST(MissionsComp, OnCompleteMission)
{
    MissionsComp ms;
    uint32_t mid = 7;

    AcceptMissionP param{ mid };
    EXPECT_EQ(kRetOK, ms.Accept(param));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));

    auto next_mission = ++mid;
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_INTERATION; ++i)
    {
        ce.condtion_ids_ = { i };
        ms.receive(ce);
        EXPECT_FALSE(ms.IsAccepted(mid));
        EXPECT_TRUE(ms.IsComplete(mid));

        auto next_mission = ++mid;
        EXPECT_TRUE(ms.IsAccepted(mid));
        EXPECT_FALSE(ms.IsComplete(mid));
    }
}

TEST(MissionsComp, AcceptNextMirroMission)
{
    MissionsComp ms;
    uint32_t mid = 7;
    auto& next_mission_set =  registry.emplace<NextTimeAcceptMission>(ms);
    AcceptMissionP param{ mid };
    EXPECT_EQ(kRetOK, ms.Accept(param));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));

    auto next_mission_id = ++mid;
    EXPECT_FALSE(ms.IsAccepted(next_mission_id));
    EXPECT_FALSE(ms.IsComplete(next_mission_id));
    EXPECT_TRUE(next_mission_set.next_time_accept_mission_id_.find(next_mission_id)
        != next_mission_set.next_time_accept_mission_id_.end());
}

TEST(MissionsComp, MissionCondition)
{
    MissionsComp ms;

    uint32_t mid = 14;
    uint32_t mid1 = 15;
    uint32_t mid2 = 16;
    AcceptMissionP param{ mid };
    EXPECT_EQ(kRetOK, ms.Accept(param));
    AcceptMissionP param1{ mid1 };
    EXPECT_EQ(kRetOK, ms.Accept(param1));
    AcceptMissionP param2{ mid2 };
    EXPECT_EQ(kRetOK, ms.Accept(param2));

    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_FALSE(ms.IsAccepted(mid1));
    EXPECT_TRUE(ms.IsComplete(mid1));
    EXPECT_FALSE(ms.IsAccepted(mid2));
    EXPECT_TRUE(ms.IsComplete(mid2));
}

TEST(MissionsComp, ConditionAmount)
{
    MissionsComp ms;

    uint32_t mid = 13;

    AcceptMissionP param{ mid };
    EXPECT_EQ(kRetOK, ms.Accept(param));

    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
}

TEST(MissionsComp, MissionRewardList)
{
    MissionsComp ms;

    registry.emplace<MissionReward>(ms);

    uint32_t mid = 12;

    AcceptMissionP param{ mid };
    EXPECT_EQ(kRetOK, ms.Accept(param));
    EXPECT_EQ(kRetMissionGetRewardNoMissionId, ms.GetReward(mid));
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_EQ(kRetOK, ms.GetReward(mid));
    EXPECT_EQ(kRetMissionGetRewardNoMissionId, ms.GetReward(mid));
    EXPECT_EQ(0, ms.can_reward_size());
}

TEST(MissionsComp, RemoveMission)
{
    MissionsComp ms;
    uint32_t mid = 12;
    AcceptMissionP param{ mid };
    EXPECT_EQ(kRetOK, ms.Accept(param));

    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.can_reward_size());
    EXPECT_EQ(1, ms.type_set_size());
    auto& type_missions = ms.classify_for_unittest();

    EXPECT_EQ(1, type_missions.find(E_CONDITION_KILL_MONSTER)->second.size());
    auto& cm = ms.complete_ids();
    ((CompleteMissionsId&)cm).mutable_can_reward_mission_id()->insert({ mid, true });
    ms.Abandon(mid);

    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(0, ms.can_reward_size());
    EXPECT_EQ(0, ms.type_set_size());
    EXPECT_EQ(0, type_missions.find(E_CONDITION_KILL_MONSTER)->second.size());
}

TEST(MissionsComp, MissionAutoReward)
{
}

TEST(MissionsComp, MissionTimeOut)
{

}

class C
{
    uint32_t status = 2;
    uint32_t id = 1;
};

class M
{
public:

	uint32_t id = 1;
	uint32_t status = 2;
    std::array<C, 5> cs;
};

using ms = std::array<C, 10000>;
std::array<ms, 10000> ps; //计算10000个玩家占用多少任务内存


int main(int argc, char** argv)
{
    Random::GetSingleton();
    condition_config::GetSingleton().load();
    mission_config::GetSingleton().load();
    testing::InitGoogleTest(&argc, argv);
    std::cout << sizeof(ps) << std::endl;
    std::cout << sizeof(Mission) << std::endl;
    std::cout << sizeof(M) << std::endl;
    return RUN_ALL_TESTS();
}

