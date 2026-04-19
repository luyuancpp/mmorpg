#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "muduo/base/Logging.h"
#include "table/code/test_table.h"
#include "table/code/testmultikey_table.h"
#include "table/code/testmultikey_table_comp.h"
#include "table/code/testmultikey_table_fk.h"
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
		LOG_INFO << row.ShortDebugString();
	}
}

TEST(ConfigTableTest, IterateBuffTable)
{
	for (auto &row : FindAllBuffTable().data())
	{
		LOG_INFO << row.ShortDebugString();
	}
}

TEST(ConfigTableTest, IterateTestTable)
{
	for (auto &row : FindAllTestTable().data())
	{
		LOG_INFO << row.ShortDebugString();
	}
}

TEST(ConfigTableTest, IterateTestMultiKeyTable)
{
	for (auto &row : FindAllTestMultiKeyTable().data())
	{
		LOG_INFO << row.ShortDebugString();
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
		LOG_INFO << it->first << ' ' << it->second->ShortDebugString();
	}
}

TEST(ConfigTableTest, MultiKeyInt32RangeQuery)
{
	auto &data = TestMultiKeyTableManager::Instance().GetMInt32KeyMap();
	auto range = data.equal_range(10);
	for (auto it = range.first; it != range.second; ++it)
	{
		LOG_INFO << it->first << ' ' << it->second->ShortDebugString();
	}
}

TEST(ConfigTableTest, MultiKeyStringRangeQuery)
{
	auto &data = TestMultiKeyTableManager::Instance().GetMStringKeyMap();
	auto range = data.equal_range("aa");
	for (auto it = range.first; it != range.second; ++it)
	{
		LOG_INFO << it->first << ' ' << it->second->ShortDebugString();
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

// ---------------------------------------------------------------------------
// Foreign key lookups
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, ScalarForeignKeyLookup)
{
	auto [row, ok] = TestMultiKeyTableManager::Instance().FindById(1);
	ASSERT_NE(row, nullptr);

	const auto *testRow = GetTestMultiKeyTestRefRow(*row);
	ASSERT_NE(testRow, nullptr);
	EXPECT_EQ(testRow->id(), row->test_ref());
}

TEST(ConfigTableTest, GroupForeignKeyLookup)
{
	auto [row, ok] = TestMultiKeyTableManager::Instance().FindById(1);
	ASSERT_NE(row, nullptr);

	auto testRows = GetTestMultiKeyTestRefsRows(*row);
	EXPECT_EQ(static_cast<int>(testRows.size()), row->test_refs_size());
	for (int i = 0; i < static_cast<int>(testRows.size()); ++i)
	{
		EXPECT_EQ(testRows[i]->id(), row->test_refs(i));
	}
}

TEST(ConfigTableTest, ForeignKeyAfterReload)
{
	auto &mgr = TestMultiKeyTableManager::Instance();

	auto [rowBefore, ok1] = mgr.FindById(1);
	ASSERT_NE(rowBefore, nullptr);
	const auto *fkBefore = GetTestMultiKeyTestRefRow(*rowBefore);
	ASSERT_NE(fkBefore, nullptr);
	const auto fkIdBefore = fkBefore->id();

	mgr.Load();
	TestTableManager::Instance().Load();

	auto [rowAfter, ok2] = mgr.FindById(1);
	ASSERT_NE(rowAfter, nullptr);
	const auto *fkAfter = GetTestMultiKeyTestRefRow(*rowAfter);
	ASSERT_NE(fkAfter, nullptr);

	EXPECT_EQ(fkAfter->id(), fkIdBefore);
	EXPECT_NE(fkAfter, fkBefore) << "reload should produce new snapshot";
}

// ---------------------------------------------------------------------------
// Secondary index (idx option)
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, SecondaryIndexByLevel)
{
	// level column has idx option -> GetByLevel(value) returns matching rows
	auto rows = TestMultiKeyTableManager::Instance().GetByLevel(1);
	EXPECT_EQ(rows.size(), 3u) << "3 rows have level=1";
	for (const auto *r : rows)
	{
		EXPECT_EQ(r->level(), 1u);
	}
}

TEST(ConfigTableTest, SecondaryIndexByLevelUnique)
{
	auto rows = TestMultiKeyTableManager::Instance().GetByLevel(0);
	EXPECT_EQ(rows.size(), 1u) << "1 row has level=0";
	if (!rows.empty())
	{
		EXPECT_EQ(rows[0]->id(), 1u);
	}
}

TEST(ConfigTableTest, SecondaryIndexByLevelNotFound)
{
	auto rows = TestMultiKeyTableManager::Instance().GetByLevel(999);
	EXPECT_TRUE(rows.empty());
}

TEST(ConfigTableTest, SecondaryIndexCountByLevel)
{
	EXPECT_EQ(TestMultiKeyTableManager::Instance().CountByLevelIndex(1), 3u);
	EXPECT_EQ(TestMultiKeyTableManager::Instance().CountByLevelIndex(100), 2u);
	EXPECT_EQ(TestMultiKeyTableManager::Instance().CountByLevelIndex(999), 0u);
}

// ---------------------------------------------------------------------------
// FK auto-indexed (scalar FK columns get a secondary index automatically)
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, ForeignKeyAutoIndex)
{
	// test_ref has fk:Test -> auto-indexed
	auto rows = TestMultiKeyTableManager::Instance().GetByTestRef(1);
	EXPECT_EQ(rows.size(), 1u);
	if (!rows.empty())
	{
		EXPECT_EQ(rows[0]->id(), 1u);
	}
}

TEST(ConfigTableTest, ForeignKeyAutoIndexMultiple)
{
	// Rows with test_ref=0 (default, no FK set)
	auto rows = TestMultiKeyTableManager::Instance().GetByTestRef(0);
	EXPECT_EQ(rows.size(), 3u) << "3 rows have test_ref=0";
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, ReverseForeignKeyLookup)
{
	// FindTestMultiKeyRowsByTestRef delegates to GetByTestRef
	auto rows = FindTestMultiKeyRowsByTestRef(1);
	EXPECT_EQ(rows.size(), 1u);
	if (!rows.empty())
	{
		EXPECT_EQ(rows[0]->test_ref(), 1u);
	}
}

TEST(ConfigTableTest, ReverseForeignKeyLookupNotFound)
{
	auto rows = FindTestMultiKeyRowsByTestRef(999);
	EXPECT_TRUE(rows.empty());
}

// ---------------------------------------------------------------------------
// Index survives hot-reload
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, SecondaryIndexAfterReload)
{
	auto &mgr = TestMultiKeyTableManager::Instance();

	auto rowsBefore = mgr.GetByLevel(1);
	ASSERT_EQ(rowsBefore.size(), 3u);

	mgr.Load();

	auto rowsAfter = mgr.GetByLevel(1);
	EXPECT_EQ(rowsAfter.size(), 3u);
	// Pointers should differ (new snapshot)
	EXPECT_NE(rowsBefore[0], rowsAfter[0]);
}

TEST(ConfigTableTest, ReverseFKAfterReload)
{
	auto &mgr = TestMultiKeyTableManager::Instance();

	auto before = FindTestMultiKeyRowsByTestRef(2);
	ASSERT_EQ(before.size(), 1u);

	mgr.Load();

	auto after = FindTestMultiKeyRowsByTestRef(2);
	EXPECT_EQ(after.size(), 1u);
	EXPECT_NE(before[0], after[0]) << "reload should produce new snapshot";
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	if (!test_config::FindAndLoadTestConfig(argc, argv))
		return 1;
	LoadTables();
	return RUN_ALL_TESTS();
}
