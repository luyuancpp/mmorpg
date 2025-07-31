#include <gtest/gtest.h>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>
#include "util/snow_flake.h"

using Guid = uint64_t;

using GuidVector = std::vector<Guid>;
using GuidSet = std::unordered_set<Guid>;

SnowFlakeAtomic idGenAtomic;
GuidVector firstV;
GuidVector secondV;
GuidVector thirdV;
static const std::size_t kTestSize = 1000000;

void emplaceToVector(GuidVector& v)
{
	for (std::size_t i = 0; i < kTestSize; ++i)
	{
		v.emplace_back(idGenAtomic.Generate());
	}
}

void generateThread1()
{
	emplaceToVector(firstV);
}

void generateThread2()
{
	emplaceToVector(secondV);
}

void generateThread3()
{
	emplaceToVector(thirdV);
}

void putVectorIntoSet(GuidSet& s, GuidVector& v)
{
	for (auto& it : v)
	{
		s.emplace(it);
	}
}

TEST(TestSnowFlake, generateNormal)
{
	GuidSet guidSet;
	GuidVector v;

	emplaceToVector(v);
	putVectorIntoSet(guidSet, v);

	EXPECT_EQ(guidSet.size(), v.size());
}

TEST(TestSnowFlakeThreadSafe, justGenerateTime)
{
	auto start = std::chrono::high_resolution_clock::now();
	Guid id = idGenAtomic.Generate();
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> duration = end - start;

	std::cout << "Single generation time: " << duration.count() << " seconds\n";
	std::cout << "Generated ID: " << id << std::endl;
}

TEST(TestSnowFlakeThreadSafe, generate)
{
	GuidSet guidSet;
	firstV.clear();
	secondV.clear();
	thirdV.clear();

	std::thread firstThread(generateThread1);
	std::thread secondThread(generateThread2);
	std::thread thirdThread(generateThread3);

	firstThread.join();
	secondThread.join();
	thirdThread.join();

	putVectorIntoSet(guidSet, firstV);
	putVectorIntoSet(guidSet, secondV);
	putVectorIntoSet(guidSet, thirdV);

	EXPECT_EQ(guidSet.size(), (firstV.size() + secondV.size() + thirdV.size()));
}

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
