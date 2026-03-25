#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "time/system/time_meter.h"

class TimeMeterUtilTest : public ::testing::Test {
protected:
	TimeMeterComp timeMeter;

	void SetUp() override {
		// initialization (if needed)
	}

	void TearDown() override {
		// cleanup (if needed)
	}
};

TEST_F(TimeMeterUtilTest, InitialExpiration) {
	timeMeter.set_duration(5); // set timer duration to 5 seconds

	TimeMeterSecondSystem::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(6)); // wait 6 seconds

	EXPECT_TRUE(TimeMeterSecondSystem::IsExpired(timeMeter));
}

TEST_F(TimeMeterUtilTest, ExtendedDuration) {
	timeMeter.set_duration(5); // set timer duration to 5 seconds

	TimeMeterSecondSystem::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(3)); // wait 3 seconds

	EXPECT_FALSE(TimeMeterSecondSystem::IsExpired(timeMeter));

	// adjust timer duration to 10 seconds
	timeMeter.set_duration(10);

	TimeMeterSecondSystem::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(6)); // wait 6 seconds

	EXPECT_FALSE(TimeMeterSecondSystem::IsExpired(timeMeter));
	EXPECT_EQ(TimeMeterSecondSystem::Remaining(timeMeter), 4); // 10s duration minus 6s elapsed = 4s remaining
}


class TimeMeterMillisecondSystemTest : public ::testing::Test {
protected:
	void SetUp() override {
		// set up a standard time
		current_time_ms = TimeMeterMillisecondSystem::GetCurrentTimeInMilliseconds();
		time_meter_comp.set_start(current_time_ms);
		time_meter_comp.set_duration(10000); // 10 seconds
	}

	uint64_t current_time_ms;
	TimeMeterComp time_meter_comp;
};

TEST_F(TimeMeterMillisecondSystemTest, RemainingTime) {
	// test remaining time
	uint64_t remaining = TimeMeterMillisecondSystem::Remaining(time_meter_comp);
	EXPECT_LE(remaining, 10000);
}

TEST_F(TimeMeterMillisecondSystemTest, IsExpired) {
	// test expiration
	EXPECT_FALSE(TimeMeterMillisecondSystem::IsExpired(time_meter_comp));

	// modify duration to cause expiration
	time_meter_comp.set_start(current_time_ms - 20000); // set start time to 20 seconds ago
	EXPECT_TRUE(TimeMeterMillisecondSystem::IsExpired(time_meter_comp));
}

TEST_F(TimeMeterMillisecondSystemTest, IsBeforeStart) {
	// test if before start time
	EXPECT_FALSE(TimeMeterMillisecondSystem::IsBeforeStart(time_meter_comp));

	// modify start time
	time_meter_comp.set_start(current_time_ms + 20000); // set to 20 seconds in the future
	EXPECT_TRUE(TimeMeterMillisecondSystem::IsBeforeStart(time_meter_comp));
}

TEST_F(TimeMeterMillisecondSystemTest, IsNotStarted) {
	// test if not started
	EXPECT_FALSE(TimeMeterMillisecondSystem::IsNotStarted(time_meter_comp));

	// modify start time
	time_meter_comp.set_start(current_time_ms + 20000); // set to 20 seconds in the future
	EXPECT_TRUE(TimeMeterMillisecondSystem::IsNotStarted(time_meter_comp));
}

TEST_F(TimeMeterMillisecondSystemTest, Reset) {
	// test reset functionality
	TimeMeterMillisecondSystem::Reset(time_meter_comp);
	uint64_t new_start = time_meter_comp.start();
	EXPECT_GE(new_start, current_time_ms);
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
