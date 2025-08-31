#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "muduo/base/Logging.h"
#include "table/code/test_table.h"
#include "table/testmultikey_table.h"
#include "table/skill_table.h"
#include "table/buff_table.h"

void LoadTables();

TEST(LoadConfigsTest, GetAbilityAllTable)
{
	for (auto& it : GetSkillAllTable().data())
	{
		LOG_INFO << it.DebugString();
	}
}

TEST(LoadConfigsTest, GetBuffAllTable)
{
	for (auto& it : GetBuffAllTable().data())
	{
		LOG_INFO << it.DebugString();
	}
}


TEST(LoadConfigsTest, GetTestAllTable)
{
    for (auto& it : GetTestAllTable().data())
    {
        LOG_INFO << it.DebugString();
    }
}

TEST(LoadConfigsTest, GetTestMultiKeyAllTable)
{
	for (auto& it : GetTestMultiKeyAllTable().data())
	{
		LOG_INFO << it.DebugString();
	}
}

TEST(LoadConfigsTest, GetTestMultiUint32ValueRangeTable)
{
	auto& data = TestMultiKeyTableManager::Instance().GetMuint32keyData();
	auto range17 = data.equal_range(17);
	for (auto it = range17.first; it != range17.second; ++it) {
		LOG_INFO << it->first << ' ' << it->second->DebugString();

	}
}

TEST(LoadConfigsTest, GetTestMultiInt32ValueRangeTable)
{
	auto& data = TestMultiKeyTableManager::Instance().GetMin32keyData();
	auto range10 = data.equal_range(10);
	for (auto it = range10.first; it != range10.second; ++it) {
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

TEST(LoadConfigsTest, GetTestMultiStringValueRangeTable)
{
	auto& data = TestMultiKeyTableManager::Instance().GetMstringkeyData();
	auto rangeaa = data.equal_range("aa");
	for (auto it = rangeaa.first; it != rangeaa.second; ++it) {
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

TEST(LoadConfigsTest, GetTestUint32ValueFindTable)
{
	auto it = TestMultiKeyTableManager::Instance().GetByUint32key(14);
	EXPECT_EQ(it.first->id(), 1);
}

TEST(LoadConfigsTest, GetTestInt32ValueAllTable)
{
	auto it = TestMultiKeyTableManager::Instance().GetByIn32key(8);
	EXPECT_EQ(it.first->id(), 1);
}

TEST(LoadConfigsTest, GetTestStringValueAllTable)
{
	auto it = TestMultiKeyTableManager::Instance().GetByStringkey("aa");
	EXPECT_EQ(it.first->id(), 1);
}

// Main function
int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);
    LoadTables();
    int ret = RUN_ALL_TESTS();
    return ret;
}
