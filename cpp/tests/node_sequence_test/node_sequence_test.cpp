#include <gtest/gtest.h>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>

#include "core/utils/id/snow_flake.h"
#include "core/utils/id/node_id_generator.h"

using GuidVector = std::vector<Guid>;
using GuidSet = std::unordered_set<Guid>;

using TransientNode32BitCompositeIdGenerator = TransientNodeCompositeIdGenerator<uint64_t, 32>;

TransientNode32BitCompositeIdGenerator idGenAtomic;

static const std::size_t kTotalIds = 0xffffff;

void GenerateIdsIntoVector(GuidVector& out)
{
	out.reserve(kTotalIds);
	for (std::size_t i = 0; i < kTotalIds; ++i)
	{
		out.emplace_back(idGenAtomic.Generate());
	}
}

// ---------------------------------------------------------------------------
// NodeCompositeIdGenerator 测试
// ---------------------------------------------------------------------------

TEST(NodeCompositeIdTest, SingleGeneration)
{
	idGenAtomic.set_node_id(1);
	Guid id = idGenAtomic.Generate();
	EXPECT_NE(id, 0);
}

TEST(NodeCompositeIdTest, AllGeneratedIdsAreUnique)
{
	for (int32_t round = 0; round < 10; ++round)
	{
		GuidVector ids;
		GenerateIdsIntoVector(ids);

		GuidSet uniqueIds(ids.begin(), ids.end());
		ASSERT_EQ(uniqueIds.size(), ids.size())
			<< "Duplicate ID detected in round " << round;
	}
}

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
