#include <gtest/gtest.h>
#include "time/system/cooldown_time.h"  

// Test Fixture
class CoolDownTimeMillisecondUtilTest  : public ::testing::Test {
protected:
	void SetUp() override {
		// Set up any necessary environment for the tests
	}

	void TearDown() override {
		// Clean up any resources after tests
	}

	uint64_t current_time_in_milliseconds() {
		return CoolDownTimeMillisecondSystem::NowMilliseconds();
	}
};

// Test for Remaining time
TEST_F(CoolDownTimeMillisecondUtilTest , RemainingTime) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(2);// 5 seconds

	// Simulate 2 seconds elapsed
	std::this_thread::sleep_for(std::chrono::seconds(2));

	uint64_t remaining_time = CoolDownTimeMillisecondSystem::Remaining(comp);
	EXPECT_GE(remaining_time, 2900); // should be at least 3 seconds
	EXPECT_LT(remaining_time, 3100); // should be less than 3.1 seconds
}

// Test for IsExpired
TEST_F(CoolDownTimeMillisecondUtilTest , IsExpired) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(1);

	// Simulate 1 second elapsed
	std::this_thread::sleep_for(std::chrono::seconds(1));

	EXPECT_TRUE(CoolDownTimeMillisecondSystem::IsExpired(comp));
}

// Test for IsBeforeStart
TEST_F(CoolDownTimeMillisecondUtilTest , IsBeforeStart) {
	CooldownTimeComp comp; // 10 seconds duration
	comp.set_start(current_time_in_milliseconds() + 5000);
	comp.set_cooldown_table_id(3);

	EXPECT_TRUE(CoolDownTimeMillisecondSystem::IsBeforeStart(comp));
}

// Test for IsNotStarted
TEST_F(CoolDownTimeMillisecondUtilTest , IsNotStarted) {
	CooldownTimeComp comp; // 10 seconds duration
	comp.set_start(current_time_in_milliseconds() + 5000);
	comp.set_cooldown_table_id(3);

	EXPECT_TRUE(CoolDownTimeMillisecondSystem::IsNotStarted(comp));
}

// Test for Reset
TEST_F(CoolDownTimeMillisecondUtilTest , Reset) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(2);// 5 seconds

	// Simulate some time passing
	std::this_thread::sleep_for(std::chrono::seconds(2));

	CoolDownTimeMillisecondSystem::Reset(comp);

	uint64_t remaining_time = CoolDownTimeMillisecondSystem::Remaining(comp);
	EXPECT_GE(remaining_time, 5000); // Should be at least 5 seconds since it was reset
}

// Test for Set and Get Duration
TEST_F(CoolDownTimeMillisecondUtilTest , SetAndGetDuration) {
	uint64_t start_time = current_time_in_milliseconds();
	CooldownTimeComp comp; // Initial duration 10 seconds
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(3);

	comp.set_cooldown_table_id(4);// Set to 20 seconds

	EXPECT_EQ(CoolDownTimeMillisecondSystem::GetDuration(comp), 20000);
}

// Test for Set and Get Start Time
TEST_F(CoolDownTimeMillisecondUtilTest , SetAndGetStartTime) {
	CooldownTimeComp comp; // Initial start time
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(3);

	uint64_t new_start_time = current_time_in_milliseconds() + 5000; // Set new start time
	CoolDownTimeMillisecondSystem::SetStartTime(comp, new_start_time);

	EXPECT_EQ(CoolDownTimeMillisecondSystem::GetStartTime(comp), new_start_time);
}

// Test for IsCooldownComplete
TEST_F(CoolDownTimeMillisecondUtilTest, IsCooldownComplete) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(1);// 1 second

	// Simulate 1 second elapsed
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	EXPECT_TRUE(CoolDownTimeMillisecondSystem::IsCooldownComplete(comp));
}

// Test for IsInCooldown
TEST_F(CoolDownTimeMillisecondUtilTest, IsInCooldown) {
	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(2);// 5 seconds

	// Simulate 2 seconds elapsed
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	EXPECT_TRUE(CoolDownTimeMillisecondSystem::IsInCooldown(comp));
}

// Test for ResetCooldown
TEST_F(CoolDownTimeMillisecondUtilTest, ResetCooldown) {

	CooldownTimeComp comp;
	comp.set_start(current_time_in_milliseconds());
	comp.set_cooldown_table_id(2);// 5 seconds

	// Simulate some time passing
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	CoolDownTimeMillisecondSystem::ResetCooldown(comp);

	uint64_t remaining_time = CoolDownTimeMillisecondSystem::Remaining(comp);
	EXPECT_GE(remaining_time, 5000); // Should be at least 5 seconds since it was reset
}

int main(int argc, char** argv)
{
	CooldownTableManager::Instance().Load();
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}