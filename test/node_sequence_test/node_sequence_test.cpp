#include <gtest/gtest.h>
#include <cassert>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>

#include "util/snow_flake.h"
#include "util/node_id_generator.h"

using GuidVector = std::vector<Guid>;
using GuidSet = std::unordered_set<Guid>;

using NodeIdGenerator32BitId = NodeIdGenerator<uint64_t, 32>;

NodeIdGenerator32BitId idGenAtomic;

static const std::size_t kTestSize = 0xffffff;

void emplaceToVector(GuidVector& v)
{
	for (std::size_t i = 0; i < kTestSize; ++i)
	{
		v.emplace_back(idGenAtomic.Generate());
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
	Guid id = idGenAtomic.Generate();
}

TEST(TestSnowFlakeThreadSafe, generate)
{
	for (int32_t i = 0; i < 100; ++i)
	{
		GuidSet guidSet;
		GuidVector firstVector;
		emplaceToVector(firstVector);
		putVectorIntoSet(guidSet, firstVector);

		assert(guidSet.size() == firstVector.size());
	}
}

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
