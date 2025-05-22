#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "muduo/base/Logging.h"
#include "test_config.h"
#include "testmultikey_config.h"
#include "skill_config.h"
#include "buff_config.h"

void LoadConfigs();

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
	auto& data = TestMultiKeyConfigurationTable::Instance().GetMuint32KeyData();
	auto range17 = data.equal_range(17);
	for (auto it = range17.first; it != range17.second; ++it) {
		LOG_INFO << it->first << ' ' << it->second->DebugString();

	}
}

TEST(LoadConfigsTest, GetTestMultiInt32ValueRangeTable)
{
	auto& data = TestMultiKeyConfigurationTable::Instance().GetMin32KeyData();
	auto range10 = data.equal_range(10);
	for (auto it = range10.first; it != range10.second; ++it) {
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

TEST(LoadConfigsTest, GetTestMultiStringValueRangeTable)
{
	auto& data = TestMultiKeyConfigurationTable::Instance().GetMstringkeyData();
	auto rangeaa = data.equal_range("aa");
	for (auto it = rangeaa.first; it != rangeaa.second; ++it) {
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

TEST(LoadConfigsTest, GetTestUint32ValueFindTable)
{
	auto it = TestMultiKeyConfigurationTable::Instance().GetByUint32Key(14);
	EXPECT_EQ(it.first->id(), 1);
}

TEST(LoadConfigsTest, GetTestInt32ValueAllTable)
{
	auto it = TestMultiKeyConfigurationTable::Instance().GetByIn32Key(8);
	EXPECT_EQ(it.first->id(), 1);
}

TEST(LoadConfigsTest, GetTestStringValueAllTable)
{
	auto it = TestMultiKeyConfigurationTable::Instance().GetByStringkey("aa");
	EXPECT_EQ(it.first->id(), 1);
}

// Main function
int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);
    LoadConfigs();
    int ret = RUN_ALL_TESTS();
    return ret;
}
