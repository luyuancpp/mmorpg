#include <gtest/gtest.h>
#include "time/system/time_cooldown.h"
#include "../test_config_helper.h"

// ---------------------------------------------------------------------------
// 技能冷却时间（毫秒级）工具测试
// ---------------------------------------------------------------------------

class CoolDownTimeMillisecondUtilTest : public ::testing::Test {
protected:
	uint64_t current_time_in_milliseconds() {
		return CoolDownTimeMillisecondSystem::NowMilliseconds();
	}
};

// 剩余冷却时间
TEST_F(CoolDownTimeMillisecondUtilTest, RemainingTime) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(2); // 5 秒冷却

	// 等待 2 秒
	std::this_thread::sleep_for(std::chrono::seconds(2));

	uint64_t remaining_time = CoolDownTimeMillisecondSystem::Remaining(comp);
	EXPECT_GE(remaining_time, 2900); // 至少剩 3 秒
	EXPECT_LT(remaining_time, 3100); // 不超过 3.1 秒
}

// 是否已过期
TEST_F(CoolDownTimeMillisecondUtilTest, IsExpired) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(1); // 1 秒冷却

	// 等待 1 秒
	std::this_thread::sleep_for(std::chrono::seconds(1));

	EXPECT_TRUE(CoolDownTimeMillisecondSystem::IsExpired(comp));
}

// 开始时间在未来，判断为“未开始”
TEST_F(CoolDownTimeMillisecondUtilTest, IsBeforeStart) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds() + 5000); // 开始时间在 5 秒后
	comp.set_cooldown_table_id(3); // 10 秒冷却

	EXPECT_TRUE(CoolDownTimeMillisecondSystem::IsBeforeStart(comp));
}

// 判断是否未开始
TEST_F(CoolDownTimeMillisecondUtilTest, IsNotStarted) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds() + 5000);
	comp.set_cooldown_table_id(3);

	EXPECT_TRUE(CoolDownTimeMillisecondSystem::IsNotStarted(comp));
}

// 重置冷却后剩余时间应恢复到满值
TEST_F(CoolDownTimeMillisecondUtilTest, Reset) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(2); // 5 秒冷却

	std::this_thread::sleep_for(std::chrono::seconds(2));

	CoolDownTimeMillisecondSystem::Reset(comp);

	uint64_t remaining_time = CoolDownTimeMillisecondSystem::Remaining(comp);
	EXPECT_GE(remaining_time, 5000); // 重置后应返回完整冷却时间
}

// 设置/获取冷却时长
TEST_F(CoolDownTimeMillisecondUtilTest, SetAndGetDuration) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(3); // 初始 10 秒

	comp.set_cooldown_table_id(4); // 改为 20 秒

	EXPECT_EQ(CoolDownTimeMillisecondSystem::GetDuration(comp), 20000);
}

// 设置/获取开始时间
TEST_F(CoolDownTimeMillisecondUtilTest, SetAndGetStartTime) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(3);

	uint64_t new_start_time = current_time_in_milliseconds() + 5000;
	CoolDownTimeMillisecondSystem::SetStartTime(comp, new_start_time);

	EXPECT_EQ(CoolDownTimeMillisecondSystem::GetStartTime(comp), new_start_time);
}

// 冷却完成判断
TEST_F(CoolDownTimeMillisecondUtilTest, IsCooldownComplete) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(1); // 1 秒冷却

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	EXPECT_TRUE(CoolDownTimeMillisecondSystem::IsCooldownComplete(comp));
}

// 冷却中判断
TEST_F(CoolDownTimeMillisecondUtilTest, IsInCooldown) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(2); // 5 秒冷却

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	EXPECT_TRUE(CoolDownTimeMillisecondSystem::IsInCooldown(comp));
}

// 调用 ResetCooldown 后剩余时间应恢复满值
TEST_F(CoolDownTimeMillisecondUtilTest, ResetCooldown) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(2); // 5 秒冷却

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	CoolDownTimeMillisecondSystem::ResetCooldown(comp);

	uint64_t remaining_time = CoolDownTimeMillisecondSystem::Remaining(comp);
	EXPECT_GE(remaining_time, 5000); // 重置后应返回完整冷却时间
}

int main(int argc, char** argv)
{
	if (!test_config::FindAndLoadTestConfig(argc, argv)) return 1;
	CooldownTableManager::Instance().Load();
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}