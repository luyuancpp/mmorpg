#include <gtest/gtest.h>

#include <cassert>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>

#include "src/server_sequence/server_sequence.h"

using game_guid_vetcor = std::vector<common::GameGuid>;
using game_guid_set = std::unordered_set<common::GameGuid>;

common::ServerSequence sf;


static const std::size_t kTestSize = 1000000;

void EmplaceToVector(game_guid_vetcor& v)
{
    for (std::size_t i = 0; i < kTestSize; ++i)
    {
        v.emplace_back(sf.Generate());
    }
}

void PutVectorInToSet(game_guid_set& s, game_guid_vetcor& v)
{
    for (auto& it : v)
    {
        s.emplace(it);
    }
}

TEST(TestSnowFlakeThreadSafe, JustGenerateTime)
{
    common::GameGuid id = sf.Generate();
}

TEST(TestSnowFlakeThreadSafe, Generate)
{
    game_guid_set guid_set;
    game_guid_vetcor first_v;
    EmplaceToVector(first_v);
    PutVectorInToSet(guid_set, first_v);

    assert(guid_set.size() == first_v.size());
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