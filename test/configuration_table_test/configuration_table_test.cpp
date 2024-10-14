#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "muduo/base/Logging.h"
#include "test_config.h"
#include "testmultikey_config.h"
#include "skill_config.h"
#include "buff_config.h"

void LoadAllConfig();

TEST(LoadAllConfigTest, GetAbilityAllTable)
{
	for (auto& it : GetSkillAllTable().data())
	{
		LOG_INFO << it.DebugString();
	}
}

TEST(LoadAllConfigTest, GetBuffAllTable)
{
	for (auto& it : GetBuffAllTable().data())
	{
		LOG_INFO << it.DebugString();
	}
}


TEST(LoadAllConfigTest, GetTestAllTable)
{
    for (auto& it : GetTestAllTable().data())
    {
        LOG_INFO << it.DebugString();
    }
}

TEST(LoadAllConfigTest, GetTestMultiKeyAllTable)
{
	for (auto& it : GetTestMultiKeyAllTable().data())
	{
		LOG_INFO << it.DebugString();
	}
}

TEST(LoadAllConfigTest, GetTestMultiUint32ValueRangeTable)
{
	auto& data = TestMultiKeyConfigurationTable::Instance().GetMuint32KeyData();
	auto range17 = data.equal_range(17);
	for (auto it = range17.first; it != range17.second; ++it) {
		LOG_INFO << it->first << ' ' << it->second->DebugString();

	}
}

TEST(LoadAllConfigTest, GetTestMultiInt32ValueRangeTable)
{
	auto& data = TestMultiKeyConfigurationTable::Instance().GetMin32KeyData();
	auto range10 = data.equal_range(10);
	for (auto it = range10.first; it != range10.second; ++it) {
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

TEST(LoadAllConfigTest, GetTestMultiStringValueRangeTable)
{
	auto& data = TestMultiKeyConfigurationTable::Instance().GetMstringkeyData();
	auto rangeaa = data.equal_range("aa");
	for (auto it = rangeaa.first; it != rangeaa.second; ++it) {
		LOG_INFO << it->first << ' ' << it->second->DebugString();
	}
}

TEST(LoadAllConfigTest, GetTestUint32ValueFindTable)
{
	auto it = TestMultiKeyConfigurationTable::Instance().GetByUint32Key(14);
	EXPECT_EQ(it.first->id(), 1);
}

TEST(LoadAllConfigTest, GetTestInt32ValueAllTable)
{
	auto it = TestMultiKeyConfigurationTable::Instance().GetByIn32Key(8);
	EXPECT_EQ(it.first->id(), 1);
}

TEST(LoadAllConfigTest, GetTestStringValueAllTable)
{
	auto it = TestMultiKeyConfigurationTable::Instance().GetByStringkey("aa");
	EXPECT_EQ(it.first->id(), 1);
}

// Main function
int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);
    LoadAllConfig();
    int ret = RUN_ALL_TESTS();
    return ret;
}
