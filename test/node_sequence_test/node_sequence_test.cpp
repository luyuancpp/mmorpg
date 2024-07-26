#include <gtest/gtest.h>
#include <cassert>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>

#include "util/snow_flake.h"

using GuidVector = std::vector<Guid>;
using GuidSet = std::unordered_set<Guid>;

using ServerSequence32 = NodeSequence<uint64_t, 32>;

ServerSequence32 sf;

static const std::size_t kTestSize = 0xffffff;

void emplaceToVector(GuidVector& v)
{
	for (std::size_t i = 0; i < kTestSize; ++i)
	{
		v.emplace_back(sf.Generate());
	}
}

void putVectorIntoSet(GuidSet& s, GuidVector& v)
{
	for (auto& it : v)
	{
		s.emplace(it);
	}
}

TEST(TestSnowFlakeThreadSafe, justGenerateTime)
{
	Guid id = sf.Generate();
}

TEST(TestSnowFlakeThreadSafe, generate)
{
	GuidSet guidSet;
	GuidVector firstVector;
	emplaceToVector(firstVector);
	putVectorIntoSet(guidSet, firstVector);

	assert(guidSet.size() == firstVector.size());
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
