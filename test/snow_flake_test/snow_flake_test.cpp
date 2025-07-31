#include <gtest/gtest.h>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>
#define ENABLE_SNOWFLAKE_TESTING

#include "util/snow_flake.h"

using Guid = uint64_t;

using GuidVector = std::vector<Guid>;
using GuidSet = std::unordered_set<Guid>;

constexpr size_t kTotal = 300'000'000;

SnowFlakeAtomic idGenAtomic;
GuidSet firstV;
GuidSet secondV;
GuidSet thirdV;

void emplaceToVector(GuidSet& v)
{
	for (std::size_t i = 0; i < kTotal; ++i)
	{
		v.emplace(idGenAtomic.Generate());
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


TEST(SnowFlakeTest, ClockRollback_SingleThread) {
	SnowFlake sf;
	sf.set_node_id(1);
	sf.set_mock_now(1000);  // 当前时间为 1000

	auto id1 = sf.Generate();
	auto c1 = ParseGuid(id1);

	sf.set_mock_now(990);  // 模拟时钟回拨
	auto id2 = sf.Generate();
	auto c2 = ParseGuid(id2);

	EXPECT_GE(c2.timestamp, c1.timestamp);  // ID 不能比之前的时间戳小
}

TEST(SnowFlakeAtomicTest, ClockRollback_Concurrent) {
	SnowFlakeAtomic sf;
	sf.set_node_id(2);
	sf.set_mock_now(2000);

	auto id1 = sf.Generate();
	auto c1 = ParseGuid(id1);

	sf.set_mock_now(1995);  // 模拟回拨
	auto id2 = sf.Generate();
	auto c2 = ParseGuid(id2);

	EXPECT_GE(c2.timestamp, c1.timestamp);  // 应修正为不小于上次
}


TEST(SnowFlakeTest, ClockRollbackSingleThread) {
	SnowFlake sf;
	sf.set_node_id(1);

	sf.set_mock_now(1000);
	Guid id1 = sf.Generate();
	auto c1 = ParseGuid(id1);

	sf.set_mock_now(990); // 模拟时钟回拨
	Guid id2 = sf.Generate();
	auto c2 = ParseGuid(id2);

	// 回拨后产生的 ID 不能小于之前
	EXPECT_GE(c2.timestamp, c1.timestamp);
	EXPECT_EQ(c2.node_id, c1.node_id);
}

TEST(SnowFlakeTest, ClockRollbackMultiThread) {
	SnowFlakeAtomic sf;
	sf.set_node_id(2);

	sf.set_mock_now(2000);
	Guid id1 = sf.Generate();
	auto c1 = ParseGuid(id1);

	sf.set_mock_now(1990); // 模拟时钟回拨
	Guid id2 = sf.Generate();
	auto c2 = ParseGuid(id2);

	// 期望修复后的时间戳不小于之前的
	EXPECT_GE(c2.timestamp, c1.timestamp);
	EXPECT_EQ(c2.node_id, c1.node_id);
}

TEST(SnowFlakeTest, Generate100MillionGUIDs_UniqueInSingleNode)
{
	std::unordered_set<Guid> ids;
	ids.reserve(kTotal);

	SnowFlake sf;
	sf.set_node_id(1);
	sf.set_epoch(kEpoch);

	for (size_t i = 0; i < kTotal; ++i) {
		Guid id = sf.Generate();

		// 检查是否重复
		auto result = ids.insert(id);
		ASSERT_TRUE(result.second) << "重复 ID 出现在索引 " << i << ", ID=" << id;

		// 可选：每 10M 输出一次进度
		if ((i + 1) % 10'000'000 == 0) {
			std::cout << "已生成 " << (i + 1) << " 个 ID..." << std::endl;
		}
	}

	EXPECT_EQ(ids.size(), kTotal);
}

// 批量 ID 生成是否正确
TEST(SnowFlakeTest, GenerateBatch)
{
	SnowFlake sf;
	sf.set_node_id(2);
	sf.set_epoch(kEpoch);

	size_t count = 1000;
	auto ids = sf.GenerateBatch(count);

	EXPECT_EQ(ids.size(), count);

	// 检查唯一性
	std::unordered_set<Guid> id_set(ids.begin(), ids.end());
	EXPECT_EQ(id_set.size(), count);
}

// 多节点 ID 是否不同
TEST(SnowFlakeTest, NodeIDAffectsGeneratedID)
{
	SnowFlake sf1;
	sf1.set_node_id(1);
	sf1.set_epoch(kEpoch);

	SnowFlake sf2;
	sf2.set_node_id(2);
	sf2.set_epoch(kEpoch);

	Guid id1 = sf1.Generate();
	Guid id2 = sf2.Generate();

	EXPECT_NE(id1, id2);
}

//单线程 + 普通生成（SnowFlake::Generate）
TEST(SnowFlakeTest, UniqueIds_SingleThread_NormalGenerate) {
	constexpr int32_t kNodeCount = 5;

	std::unordered_set<Guid> all_ids;
	all_ids.reserve(kNodeCount * kTotal);

	for (int32_t node = 0; node < kNodeCount; ++node) {
		SnowFlake sf;
		sf.set_node_id(static_cast<uint16_t>(node));

		for (int32_t i = 0; i < kTotal; ++i) {
			Guid id = sf.Generate();
			auto [_, inserted] = all_ids.insert(id);
			ASSERT_TRUE(inserted) << "Duplicate ID from node " << node;
		}
	}

	EXPECT_EQ(all_ids.size(), kNodeCount * kTotal);
}

//单线程 + 批量生成（SnowFlake::GenerateBatch）
TEST(SnowFlakeTest, UniqueIds_SingleThread_BatchGenerate) {
	constexpr int32_t kNodeCount = 5;

	std::unordered_set<Guid> all_ids;
	all_ids.reserve(kNodeCount * kTotal);

	for (int32_t node = 0; node < kNodeCount; ++node) {
		SnowFlake sf;
		sf.set_node_id(static_cast<uint16_t>(node));

		auto ids = sf.GenerateBatch(kTotal);
		for (const auto& id : ids) {
			auto [_, inserted] = all_ids.insert(id);
			ASSERT_TRUE(inserted) << "Duplicate ID from node " << node;
		}
	}

	EXPECT_EQ(all_ids.size(), kNodeCount * kTotal);
}

//多线程 + 普通生成（SnowFlakeAtomic::Generate）
TEST(SnowFlakeTest, UniqueIds_MultiThread_NormalGenerate) {
	constexpr int32_t kNodeCount = 5;

	std::unordered_set<Guid> all_ids;
	std::mutex id_mutex;

	std::vector<std::thread> threads;

	for (int32_t node = 0; node < kNodeCount; ++node) {
		threads.emplace_back([node, &all_ids, &id_mutex]() {
			SnowFlakeAtomic sf;
			sf.set_node_id(static_cast<uint16_t>(node));

			for (int32_t i = 0; i < kTotal; ++i) {
				Guid id = sf.Generate();
				std::lock_guard<std::mutex> lock(id_mutex);
				auto [_, inserted] = all_ids.insert(id);
				ASSERT_TRUE(inserted) << "Duplicate ID in thread node " << node;
			}
			});
	}

	for (auto& t : threads) {
		t.join();
	}

	EXPECT_EQ(all_ids.size(), kNodeCount * kTotal);
}

//多线程 + 批量生成（SnowFlakeAtomic::GenerateBatch）
TEST(SnowFlakeTest, UniqueIds_MultiThread_BatchGenerate) {
	constexpr int32_t kNodeCount = 5;

	std::unordered_set<Guid> all_ids;
	std::mutex id_mutex;

	std::vector<std::thread> threads;

	for (int32_t node = 0; node < kNodeCount; ++node) {
		threads.emplace_back([node, &all_ids, &id_mutex]() {
			SnowFlakeAtomic sf;
			sf.set_node_id(static_cast<uint16_t>(node));

			auto ids = sf.GenerateBatch(kTotal);
			std::lock_guard<std::mutex> lock(id_mutex);
			for (const auto& id : ids) {
				auto [_, inserted] = all_ids.insert(id);
				ASSERT_TRUE(inserted) << "Duplicate ID in thread node " << node;
			}
			});
	}

	for (auto& t : threads) {
		t.join();
	}

	EXPECT_EQ(all_ids.size(), kNodeCount * kTotal);
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

	EXPECT_EQ(guidSet.size(), (firstV.size() + secondV.size() + thirdV.size()));
}

TEST(SnowFlakeTest, SequentialOrder) {
	SnowFlake gen;
	gen.set_node_id(1);
	Guid last = gen.Generate();

	for (int32_t i = 0; i < 1000; ++i) {
		Guid next = gen.Generate();
		ASSERT_GT(next, last) << "ID should be monotonically increasing";
		last = next;
	}
}

TEST(SnowFlakeTest, MultiThreadUniqueness) {
	constexpr int32_t THREAD_COUNT = 8;
	constexpr int32_t IDS_PER_THREAD = 5000;

	SnowFlakeAtomic gen;
	gen.set_node_id(2);
	std::unordered_set<Guid> ids;
	std::mutex mutex;

	auto worker = [&]() {
		std::unordered_set<Guid> local_ids;
		for (int32_t i = 0; i < IDS_PER_THREAD; ++i) {
			local_ids.insert(gen.Generate());
		}

		std::lock_guard<std::mutex> lock(mutex);
		for (const auto& id : local_ids) {
			auto [_, inserted] = ids.insert(id);
			ASSERT_TRUE(inserted) << "Duplicate ID detected in multithreaded context";
		}
		};

	std::vector<std::thread> threads;
	for (int32_t i = 0; i < THREAD_COUNT; ++i) {
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
	ASSERT_GT(real_time, 1600000000); // Unix 时间戳 sanity check
}

// 测试批量生成的唯一性
TEST(SnowFlakeTest, GenerateBatchUnique)
{
	SnowFlake generator;
	generator.set_node_id(1);

	const size_t batch_size = 10000;
	std::vector<Guid> ids = generator.GenerateBatch(batch_size);

	ASSERT_EQ(ids.size(), batch_size);

	std::unordered_set<Guid> unique_ids(ids.begin(), ids.end());
	EXPECT_EQ(unique_ids.size(), ids.size()) << "IDs are not unique";
}

// 测试多次批量生成的正确性
TEST(SnowFlakeTest, MultipleBatchGeneration)
{
	SnowFlakeAtomic generator;
	generator.set_node_id(123);

	const size_t batch1 = 5000;
	const size_t batch2 = 8000;

	std::vector<Guid> ids1 = generator.GenerateBatch(batch1);
	std::vector<Guid> ids2 = generator.GenerateBatch(batch2);

	std::unordered_set<Guid> all(ids1.begin(), ids1.end());
	all.insert(ids2.begin(), ids2.end());

	EXPECT_EQ(all.size(), batch1 + batch2) << "Duplicate IDs found across batches";
}

// 测试解析 ID 成分
TEST(SnowFlakeTest, GuidParsing)
{
	SnowFlake generator;
	generator.set_node_id(42);
	Guid id = generator.Generate();

	auto parsed = ParseGuid(id);

	EXPECT_EQ(parsed.node_id, 42);
	EXPECT_LT(parsed.sequence, (1U << kStepBits));
	EXPECT_GT(parsed.timestamp, 0);
}

// 并发压力测试：多个线程并发批量生成，确保 ID 唯一
TEST(SnowFlakeTest, ConcurrentBatchGeneration)
{
	constexpr size_t kThreads = 8;
	constexpr size_t kIDsPerThread = 10000;
	constexpr size_t kTotalIDs = kThreads * kIDsPerThread;

	SnowFlakeAtomic generator;
	generator.set_node_id(999);

	std::vector<std::thread> threads;
	std::vector<Guid> all_ids;
	all_ids.reserve(kTotalIDs);

	std::mutex mutex;  // 保护 all_ids

	auto worker = [&]() {
		auto ids = generator.GenerateBatch(kIDsPerThread);
		std::lock_guard<std::mutex> lock(mutex);
		all_ids.insert(all_ids.end(), ids.begin(), ids.end());
		};

	for (size_t i = 0; i < kThreads; ++i) {
		threads.emplace_back(worker);
	}

	for (auto& t : threads) {
		t.join();
	}

	// 验证总数
	ASSERT_EQ(all_ids.size(), kTotalIDs);

	// 验证唯一性
	std::unordered_set<Guid> unique_ids(all_ids.begin(), all_ids.end());
	EXPECT_EQ(unique_ids.size(), all_ids.size()) << "并发生成的 ID 有重复";
}

int32_t main(int32_t argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
