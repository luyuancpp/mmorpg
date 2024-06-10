#include <gtest/gtest.h>

#include <cassert>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>

#include "src/util/snow_flake.h"

using guid_vetcor = std::vector<Guid>;
using guid_set = std::unordered_set<Guid>;

using ServerSequence32 = NodeSequence<uint64_t, 32>;

ServerSequence32 sf;


static const std::size_t kTestSize = 0xffffff;

void EmplaceToVector(guid_vetcor& v)
{
    for (std::size_t i = 0; i < kTestSize; ++i)
    {
        v.emplace_back(sf.Generate());
    }
}

void PutVectorInToSet(guid_set& s, guid_vetcor& v)
{
    for (auto& it : v)
    {
        s.emplace(it);
    }
}

TEST(TestSnowFlakeThreadSafe, JustGenerateTime)
{
    Guid id = sf.Generate();
}

TEST(TestSnowFlakeThreadSafe, Generate)
{
    guid_set guid_set;
    guid_vetcor first_v;
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