#include <gtest/gtest.h>

#include <cassert>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>

#include "src/snow_flake/snow_flake.h"

using game_guid_vetcor = std::vector<common::GameGuid>;
using game_guid_set = std::unordered_set<common::GameGuid>;

common::SnowFlake sf;
game_guid_vetcor first_v;
game_guid_vetcor second_v;
game_guid_vetcor third_v;
static const std::size_t kTestSize = 1000000;

void EmplaceToVector(game_guid_vetcor& v)
{
    for (std::size_t i = 0; i < kTestSize; ++i)
    {
        v.emplace_back(sf.Generate());
    }
}

void GenerateThread1()
{
    EmplaceToVector(first_v);
}

void GenerateThread2()
{
    EmplaceToVector(second_v);
}

void GenerateThread3()
{
    EmplaceToVector(third_v);
}

void PutVectorInToSet(game_guid_set& s, game_guid_vetcor& v)
{
    for (auto& it : v)
    {
        s.emplace(it);
    }
}

TEST(TestSnowFlake, JustGenerateTime)
{
    common::GameGuid id = sf.Generate();
}

TEST(TestSnowFlake, GenerateTime)
{
    common::SnowFlake sf;
    time_t t = sf.GetNow();
    std::cout << t << std::endl;
    common::GameGuid id = sf.Generate();
    std::cout << id << std::endl;
}

TEST(TestSnowFlake, Generate)
{
    game_guid_set guid_set;
    first_v.clear();
    second_v.clear();
    third_v.clear();

    auto first_cb = std::bind(GenerateThread1);
    auto second_cb = std::bind(GenerateThread2);
    auto third_cb = std::bind(GenerateThread3);
    std::thread first_thread(first_cb);
    std::thread second_thread(second_cb);
    std::thread third_thread(third_cb);

    first_thread.join();
    second_thread.join();
    third_thread.join();

    PutVectorInToSet(guid_set, first_v);
    PutVectorInToSet(guid_set, second_v);
    PutVectorInToSet(guid_set, third_v);

    assert(guid_set.size() == (first_v.size() + second_v.size() + third_v.size()));
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    while (true)
    {
        RUN_ALL_TESTS();
    }
    return RUN_ALL_TESTS();
}