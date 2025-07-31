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


TEST(SnowFlakeTest, Generate100MillionGUIDs_UniqueInSingleNode)
{
	constexpr size_t total = 100'000'0000;
	std::unordered_set<Guid> ids;
	ids.reserve(total);

	SnowFlake sf;
	sf.set_node_id(1);
	sf.set_epoch(kEpoch);

	for (size_t i = 0; i < total; ++i) {
		Guid id = sf.Generate();

		// ����Ƿ��ظ�
		auto result = ids.insert(id);
		ASSERT_TRUE(result.second) << "�ظ� ID ���������� " << i << ", ID=" << id;

		// ��ѡ��ÿ 10M ���һ�ν���
		if ((i + 1) % 10'000'000 == 0) {
			std::cout << "������ " << (i + 1) << " �� ID..." << std::endl;
		}
	}

	EXPECT_EQ(ids.size(), total);
}

// ���� ID �����Ƿ���ȷ
TEST(SnowFlakeTest, GenerateBatch)
{
	SnowFlake sf;
	sf.set_node_id(2);
	sf.set_epoch(kEpoch);

	size_t count = 1000;
	auto ids = sf.GenerateBatch(count);

	EXPECT_EQ(ids.size(), count);

	// ���Ψһ��
	std::unordered_set<Guid> id_set(ids.begin(), ids.end());
	EXPECT_EQ(id_set.size(), count);
}

// ��ڵ� ID �Ƿ�ͬ
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

// ģ��ʱ��ز����߼���ʾ��ʵ�ʲ����� mock��
TEST(SnowFlakeTest, HandleClockRollback)
{
	SnowFlake sf;
	sf.set_node_id(1);
	sf.set_epoch(kEpoch);

	Guid id1 = sf.Generate();

	// ģ��ϵͳʱ��ز��������ע��� mock��
	// �����޷���ȷģ�⣬��������ֶ������ڲ��������в���
	// ���� WaitNextTime(last_time_) �ķ���ֵ > last_time_

	EXPECT_NO_THROW({
		Guid id2 = sf.Generate();
		EXPECT_GT(id2, id1);
		});
}



TEST(SnowFlakeAtomicTest, StepAutoIncrementInSameSecond)
{
	SnowFlakeAtomic sf;
	sf.set_node_id(1);

	// �ȴ�ʱ����뵽���롱��ȷ��������������ͬһ��
	auto now = std::chrono::system_clock::now();
	auto now_sec = std::chrono::time_point_cast<std::chrono::seconds>(now);
	auto wait_until = now_sec + std::chrono::seconds(1); // ��һ����

	std::this_thread::sleep_until(wait_until);

	// ���ڿ�ʼ�ڡ�ͬһ�롱�ڿ������ɶ�� ID
	std::vector<Guid> ids;
	for (int i = 0; i < 100; ++i) {
		ids.push_back(sf.Generate());
	}

	// ��� sequence �Ƿ������step �� 0 ��ʼ
	for (size_t i = 0; i < ids.size(); ++i) {
		SnowFlakeComponents c = ParseGuid(ids[i]);
		LOG_INFO << "Index: " << i << ", step: " << c.sequence;
		EXPECT_EQ(c.sequence, i); // step Ӧ�õ�������
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
	ASSERT_GT(real_time, 1600000000); // Unix ʱ��� sanity check
}

// �����������ɵ�Ψһ��
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

// ���Զ���������ɵ���ȷ��
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

// ���Խ��� ID �ɷ�
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

// ����ѹ�����ԣ�����̲߳����������ɣ�ȷ�� ID Ψһ
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

	std::mutex mutex;  // ���� all_ids

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

	// ��֤����
	ASSERT_EQ(all_ids.size(), kTotalIDs);

	// ��֤Ψһ��
	std::unordered_set<Guid> unique_ids(all_ids.begin(), all_ids.end());
	EXPECT_EQ(unique_ids.size(), all_ids.size()) << "�������ɵ� ID ���ظ�";
}

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
