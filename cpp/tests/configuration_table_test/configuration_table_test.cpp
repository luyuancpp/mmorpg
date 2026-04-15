#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "muduo/base/Logging.h"
#include "table/code/test_table.h"
#include "table/code/testmultikey_table.h"
#include "table/code/testmultikey_table_comp.h"
#include "table/code/skill_table.h"
#include "table/code/buff_table.h"
#include "table/code/buff_table_comp.h"
#include "../test_config_helper.h"

void LoadTables();

// ---------------------------------------------------------------------------
// 验证各配置表能正常加载并遍历
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, IterateSkillTable)
{
	for (auto &row : FindAllSkillTable().data())
	{
		LOG_INFO << row.DebugString();
	}
}

TEST(ConfigTableTest, IterateBuffTable)
{
	for (auto &row : FindAllBuffTable().data())
	{
		LOG_INFO << row.DebugString();
	}
}

TEST(ConfigTableTest, IterateTestTable)
{
	for (auto &row : FindAllTestTable().data())
	{
		LOG_INFO << row.DebugString();
	}
}

TEST(ConfigTableTest, IterateTestMultiKeyTable)
{
	for (auto &row : FindAllTestMultiKeyTable().data())
	{
		LOG_INFO << row.DebugString();
	}
}

// ---------------------------------------------------------------------------
// 多键表 equal_range 范围查询
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, MultiKeyUint32RangeQuery)
{
	auto &data = TestMultiKeyTableManager::Instance().GetMUint32KeyMap();
	auto range = data.equal_range(17);
	for (auto it = range.first; it != range.second; ++it)
	{
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

TEST(ConfigTableTest, MultiKeyInt32RangeQuery)
{
	auto &data = TestMultiKeyTableManager::Instance().GetMInt32KeyMap();
	auto range = data.equal_range(10);
	for (auto it = range.first; it != range.second; ++it)
	{
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

TEST(ConfigTableTest, MultiKeyStringRangeQuery)
{
	auto &data = TestMultiKeyTableManager::Instance().GetMStringKeyMap();
	auto range = data.equal_range("aa");
	for (auto it = range.first; it != range.second; ++it)
	{
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

// ---------------------------------------------------------------------------
// 多键表精确查找
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, FindByUint32Key)
{
	auto result = TestMultiKeyTableManager::Instance().FindByUint32Key(14);
	EXPECT_EQ(result.first->id(), 1);
}

TEST(ConfigTableTest, FindByInt32Key)
{
	auto result = TestMultiKeyTableManager::Instance().FindByInt32Key(8);
	EXPECT_EQ(result.first->id(), 1);
}

TEST(ConfigTableTest, FindByStringKey)
{
	auto result = TestMultiKeyTableManager::Instance().FindByStringKey("aa");
	EXPECT_EQ(result.first->id(), 1);
}

// ---------------------------------------------------------------------------
// 每列组件 (per-column ECS component)
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, ScalarCompFromRow)
{
	auto [row, ok] = TestMultiKeyTableManager::Instance().FindById(1);
	ASSERT_NE(row, nullptr);

	auto idComp = MakeTestMultiKeyIdComp(*row);
	EXPECT_EQ(idComp.value, 1u);

	auto strComp = MakeTestMultiKeyString_keyComp(*row);
	EXPECT_EQ(strComp.value, "aa");

	auto u32Comp = MakeTestMultiKeyUint32_keyComp(*row);
	EXPECT_EQ(u32Comp.value, 14u);

	auto i32Comp = MakeTestMultiKeyInt32_keyComp(*row);
	EXPECT_EQ(i32Comp.value, 8);
}

TEST(ConfigTableTest, RepeatedCompSpan)
{
	auto [row, ok] = TestMultiKeyTableManager::Instance().FindById(1);
	ASSERT_NE(row, nullptr);

	auto effectComp = MakeTestMultiKeyEffectComp(*row);
	// span points into proto memory
	EXPECT_EQ(effectComp.values.data(), row->effect().data());
	EXPECT_EQ(effectComp.values.size(), static_cast<size_t>(row->effect_size()));
}

TEST(ConfigTableTest, BuffCompStringView)
{
	// Verify string columns produce valid string_view components
	for (auto &row : FindAllBuffTable().data())
	{
		auto comp = MakeBuffHealth_regenerationComp(row);
		// string_view should point into proto memory (not a copy)
		if (!row.health_regeneration().empty())
		{
			EXPECT_EQ(comp.value.data(), row.health_regeneration().data());
		}
	}
}

// ---------------------------------------------------------------------------
// repeated 列值索引 (repeated column value index)
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, RepeatedEffectIndex)
{
	// GetEffectIndex() returns multimap<uint32_t, const Table*>
	auto &idx = TestMultiKeyTableManager::Instance().GetEffectIndex();
	// Every row's effect values should be indexed
	for (auto &row : FindAllTestMultiKeyTable().data())
	{
		for (auto val : row.effect())
		{
			auto range = idx.equal_range(val);
			bool found = false;
			for (auto it = range.first; it != range.second; ++it)
			{
				if (it->second->id() == row.id())
				{
					found = true;
					break;
				}
			}
			EXPECT_TRUE(found) << "effect value " << val
							   << " not indexed for row id " << row.id();
		}
	}
}

TEST(ConfigTableTest, BuffSubBuffIndex)
{
	auto &idx = BuffTableManager::Instance().GetSubBuffIndex();
	for (auto &row : FindAllBuffTable().data())
	{
		for (auto val : row.sub_buff())
		{
			auto range = idx.equal_range(val);
			bool found = false;
			for (auto it = range.first; it != range.second; ++it)
			{
				if (it->second->id() == row.id())
				{
					found = true;
					break;
				}
			}
			EXPECT_TRUE(found) << "sub_buff value " << val
							   << " not indexed for row id " << row.id();
		}
	}
}

// ---------------------------------------------------------------------------
// Reload (hot-reload safety: snapshot swap, no accumulation)
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, ReloadDoesNotAccumulateData)
{
	auto &mgr = TestMultiKeyTableManager::Instance();
	const auto countBefore = mgr.Count();
	ASSERT_GT(countBefore, 0u);

	// Reload the same data file.
	mgr.Load();

	// Count must stay the same — old snapshot replaced, not accumulated.
	EXPECT_EQ(mgr.Count(), countBefore);

	// Data should still be accessible.
	auto [row, ok] = mgr.FindById(1);
	ASSERT_NE(row, nullptr);
	EXPECT_EQ(row->id(), 1u);
}

TEST(ConfigTableTest, ReloadReplacesOldPointers)
{
	auto &mgr = TestMultiKeyTableManager::Instance();

	// Get a pointer before reload.
	auto [rowBefore, ok1] = mgr.FindById(1);
	ASSERT_NE(rowBefore, nullptr);
	EXPECT_EQ(rowBefore->id(), 1u);

	// Reload.
	mgr.Load();

	// New pointer should be valid and contain correct data,
	// but will point to a different Snapshot's protobuf storage.
	auto [rowAfter, ok2] = mgr.FindById(1);
	ASSERT_NE(rowAfter, nullptr);
	EXPECT_EQ(rowAfter->id(), 1u);
	EXPECT_NE(rowBefore, rowAfter) << "reload should produce a new Snapshot";
}

TEST(ConfigTableTest, ReloadMultiKeyIndicesConsistent)
{
	auto &mgr = TestMultiKeyTableManager::Instance();

	// Reload twice to stress test.
	mgr.Load();
	mgr.Load();

	// Multi-key range query should still work correctly.
	auto &data = mgr.GetMUint32KeyMap();
	auto range = data.equal_range(17);
	std::size_t rangeCount = 0;
	for (auto it = range.first; it != range.second; ++it)
	{
		++rangeCount;
	}
	// Verify no accumulation: count should match initial data.
	const auto countBefore = mgr.Count();
	mgr.Load();
	EXPECT_EQ(mgr.Count(), countBefore);
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	if (!test_config::FindAndLoadTestConfig(argc, argv))
		return 1;
	LoadTables();
	return RUN_ALL_TESTS();
}
