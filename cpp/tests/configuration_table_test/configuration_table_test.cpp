#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "muduo/base/Logging.h"
#include "table/code/test_table.h"
#include "table/code/testmultikey_table.h"
#include "table/code/skill_table.h"
#include "table/code/buff_table.h"
#include "../test_config_helper.h"

void LoadTables();

// ---------------------------------------------------------------------------
// 验证各配置表能正常加载并遍历
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, IterateSkillTable)
{
	for (auto &row : GetSkillAllTable().data())
	{
		LOG_INFO << row.DebugString();
	}
}

TEST(ConfigTableTest, IterateBuffTable)
{
	for (auto &row : GetBuffAllTable().data())
	{
		LOG_INFO << row.DebugString();
	}
}

TEST(ConfigTableTest, IterateTestTable)
{
	for (auto &row : GetTestAllTable().data())
	{
		LOG_INFO << row.DebugString();
	}
}

TEST(ConfigTableTest, IterateTestMultiKeyTable)
{
	for (auto &row : GetTestMultiKeyAllTable().data())
	{
		LOG_INFO << row.DebugString();
	}
}

// ---------------------------------------------------------------------------
// 多键表 equal_range 范围查询
// ---------------------------------------------------------------------------

TEST(ConfigTableTest, MultiKeyUint32RangeQuery)
{
	auto &data = TestMultiKeyTableManager::Instance().GetMuint32keyData();
	auto range = data.equal_range(17);
	for (auto it = range.first; it != range.second; ++it)
	{
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

TEST(ConfigTableTest, MultiKeyInt32RangeQuery)
{
	auto &data = TestMultiKeyTableManager::Instance().GetMin32keyData();
	auto range = data.equal_range(10);
	for (auto it = range.first; it != range.second; ++it)
	{
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

TEST(ConfigTableTest, MultiKeyStringRangeQuery)
{
	auto &data = TestMultiKeyTableManager::Instance().GetMstringkeyData();
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
	auto result = TestMultiKeyTableManager::Instance().GetByUint32key(14);
	EXPECT_EQ(result.first->id(), 1);
}

TEST(ConfigTableTest, FindByInt32Key)
{
	auto result = TestMultiKeyTableManager::Instance().GetByIn32key(8);
	EXPECT_EQ(result.first->id(), 1);
}

TEST(ConfigTableTest, FindByStringKey)
{
	auto result = TestMultiKeyTableManager::Instance().GetByStringkey("aa");
	EXPECT_EQ(result.first->id(), 1);
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	if (!test_config::FindAndLoadTestConfig(argc, argv))
		return 1;
	LoadTables();
	return RUN_ALL_TESTS();
}
