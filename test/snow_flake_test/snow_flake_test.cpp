#include <gtest/gtest.h>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>
#include "util/snow_flake.h"

using Guid = uint64_t;

using guid_vector = std::vector<Guid>;
using guid_set = std::unordered_set<Guid>;

SnowFlakeThreadSafe sf;
guid_vector first_v;
guid_vector second_v;
guid_vector third_v;
static const std::size_t kTestSize = 1000000;

void EmplaceToVector(guid_vector& v)
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

void PutVectorInToSet(guid_set& s, guid_vector& v)
{
	for (auto& it : v)
	{
		s.emplace(it);
	}
}

TEST(TestSnowFlake, GenerateNormal)
{
	guid_set guid_set;
	guid_vector v;

	EmplaceToVector(v);
	PutVectorInToSet(guid_set, v);

	EXPECT_EQ(guid_set.size(), v.size());
}

TEST(TestSnowFlakeThreadSafe, JustGenerateTime)
{
	auto start = std::chrono::high_resolution_clock::now();
	Guid id = sf.Generate();
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> duration = end - start;

	std::cout << "Single generation time: " << duration.count() << " seconds\n";
	std::cout << "Generated ID: " << id << std::endl;
}

TEST(TestSnowFlakeThreadSafe, Generate)
{
	guid_set guid_set;
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

	EXPECT_EQ(guid_set.size(), (first_v.size() + second_v.size() + third_v.size()));
}

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
