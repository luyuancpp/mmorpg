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

TEST(SnowFlakeTest, SequentialOrder) {
	SnowFlake gen;
	gen.set_node_id(1);
	Guid last = gen.Generate();

	for (int i = 0; i < 1000; ++i) {
		Guid next = gen.Generate();
		ASSERT_GT(next, last) << "ID should be monotonically increasing";
		last = next;
	}
}

TEST(SnowFlakeTest, MultiThreadUniqueness) {
	constexpr int THREAD_COUNT = 8;
	constexpr int IDS_PER_THREAD = 5000;

	SnowFlakeAtomic gen;
	gen.set_node_id(2);
	std::unordered_set<Guid> ids;
	std::mutex mutex;

	auto worker = [&]() {
		std::unordered_set<Guid> local_ids;
		for (int i = 0; i < IDS_PER_THREAD; ++i) {
			local_ids.insert(gen.Generate());
		}

		std::lock_guard<std::mutex> lock(mutex);
		for (const auto& id : local_ids) {
			auto [_, inserted] = ids.insert(id);
			ASSERT_TRUE(inserted) << "Duplicate ID detected in multithreaded context";
		}
		};

	std::vector<std::thread> threads;
	for (int i = 0; i < THREAD_COUNT; ++i) {
		threads.emplace_back(worker);
	}

	for (auto& t : threads) {
		t.join();
	}

	ASSERT_EQ(ids.size(), THREAD_COUNT * IDS_PER_THREAD);
}

TEST(SnowFlakeTest, IdParsing) {
	SnowFlake gen;
	gen.set_node_id(3);
	Guid id = gen.Generate();
	auto components = ParseGuid(id);
	auto real_time = GetRealTimeFromGuid(id);

	ASSERT_EQ(components.node_id, 3);
	ASSERT_LT(components.sequence, (1ULL << kStepBits));
	ASSERT_GT(real_time, 1600000000); // Unix Ê±¼ä´Á sanity check
}


int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
