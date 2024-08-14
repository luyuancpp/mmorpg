#include <gtest/gtest.h>
#include "time/util/cooldown_time_util.h"  // 假设你将上面的 CoolDownTimeMillisecondUtil 定义在这个头文件中

// Test Fixture
class CoolDownTimeUtilTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Set up any necessary environment for the tests
	}

	void TearDown() override {
		// Clean up any resources after tests
	}

	uint64_t current_time_in_milliseconds() {
		return CoolDownTimeMillisecondUtil::GetCurrentTimeInMilliseconds();
	}
};

// Test for Remaining time
TEST_F(CoolDownTimeUtilTest, RemainingTime) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(2);// 5 seconds

	// Simulate 2 seconds elapsed
	std::this_thread::sleep_for(std::chrono::seconds(2));

	uint64_t remaining_time = CoolDownTimeMillisecondUtil::Remaining(comp);
	EXPECT_GE(remaining_time, 2900); // should be at least 3 seconds
	EXPECT_LT(remaining_time, 3100); // should be less than 3.1 seconds
}

// Test for IsExpired
TEST_F(CoolDownTimeUtilTest, IsExpired) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(1);

	// Simulate 1 second elapsed
	std::this_thread::sleep_for(std::chrono::seconds(1));

	EXPECT_TRUE(CoolDownTimeMillisecondUtil::IsExpired(comp));
}

// Test for IsBeforeStart
TEST_F(CoolDownTimeUtilTest, IsBeforeStart) {
	CooldownTimeComp comp; // 10 seconds duration
	comp.set_start(current_time_in_milliseconds() + 5000);
	comp.set_cooldown_table_id(3);

	EXPECT_TRUE(CoolDownTimeMillisecondUtil::IsBeforeStart(comp));
}

// Test for IsNotStarted
TEST_F(CoolDownTimeUtilTest, IsNotStarted) {
	CooldownTimeComp comp; // 10 seconds duration
	comp.set_start(current_time_in_milliseconds() + 5000);
	comp.set_cooldown_table_id(3);

	EXPECT_TRUE(CoolDownTimeMillisecondUtil::IsNotStarted(comp));
}

// Test for Reset
TEST_F(CoolDownTimeUtilTest, Reset) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(2);// 5 seconds

	// Simulate some time passing
	std::this_thread::sleep_for(std::chrono::seconds(2));

	CoolDownTimeMillisecondUtil::Reset(comp);

	uint64_t remaining_time = CoolDownTimeMillisecondUtil::Remaining(comp);
	EXPECT_GE(remaining_time, 5000); // Should be at least 5 seconds since it was reset
}

// Test for Set and Get Duration
TEST_F(CoolDownTimeUtilTest, SetAndGetDuration) {
	uint64_t start_time = current_time_in_milliseconds();
	CooldownTimeComp comp; // Initial duration 10 seconds
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(3);

	comp.set_cooldown_table_id(4);// Set to 20 seconds

	EXPECT_EQ(CoolDownTimeMillisecondUtil::GetDuration(comp), 20000);
}

// Test for Set and Get Start Time
TEST_F(CoolDownTimeUtilTest, SetAndGetStartTime) {
	CooldownTimeComp comp; // Initial start time
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(3);

	uint64_t new_start_time = current_time_in_milliseconds() + 5000; // Set new start time
	CoolDownTimeMillisecondUtil::SetStartTime(comp, new_start_time);

	EXPECT_EQ(CoolDownTimeMillisecondUtil::GetStartTime(comp), new_start_time);
}
