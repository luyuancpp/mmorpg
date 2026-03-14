#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "time/system/time_meter.h"

class TimeMeterUtilTest : public ::testing::Test {
protected:
	TimeMeterComp timeMeter;

	void SetUp() override {
		// 初始化代码（如果需要）
	}

	void TearDown() override {
		// 清理代码（如果需要）
	}
};

TEST_F(TimeMeterUtilTest, InitialExpiration) {
	timeMeter.set_duration(5); // 设置时间测量器持续时间为5秒

	TimeMeterSecondSystem::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(6)); // 等待6秒

	EXPECT_TRUE(TimeMeterSecondSystem::IsExpired(timeMeter));
}

TEST_F(TimeMeterUtilTest, ExtendedDuration) {
	timeMeter.set_duration(5); // 设置时间测量器持续时间为5秒

	TimeMeterSecondSystem::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(3)); // 等待3秒

	EXPECT_FALSE(TimeMeterSecondSystem::IsExpired(timeMeter));

	// 调整时间测量器的持续时间为10秒
	timeMeter.set_duration(10);

	TimeMeterSecondSystem::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(6)); // 等待6秒

	EXPECT_FALSE(TimeMeterSecondSystem::IsExpired(timeMeter));
	EXPECT_EQ(TimeMeterSecondSystem::Remaining(timeMeter), 4); // 10秒持续时间减去6秒，剩余4秒
}


class TimeMeterMillisecondSystemTest : public ::testing::Test {
protected:
	void SetUp() override {
		// 设置一个标准时间
		current_time_ms = TimeMeterMillisecondSystem::GetCurrentTimeInMilliseconds();
		time_meter_comp.set_start(current_time_ms);
		time_meter_comp.set_duration(10000); // 10秒
	}

	uint64_t current_time_ms;
	TimeMeterComp time_meter_comp;
};

TEST_F(TimeMeterMillisecondSystemTest, RemainingTime) {
	// 测试剩余时间
	uint64_t remaining = TimeMeterMillisecondSystem::Remaining(time_meter_comp);
	EXPECT_LE(remaining, 10000);
}

TEST_F(TimeMeterMillisecondSystemTest, IsExpired) {
	// 测试是否超时
	EXPECT_FALSE(TimeMeterMillisecondSystem::IsExpired(time_meter_comp));

	// 修改持续时间以使其超时
	time_meter_comp.set_start(current_time_ms - 20000); // 起始时间设置为20秒前
	EXPECT_TRUE(TimeMeterMillisecondSystem::IsExpired(time_meter_comp));
}

TEST_F(TimeMeterMillisecondSystemTest, IsBeforeStart) {
	// 测试是否在开始时间之前
	EXPECT_FALSE(TimeMeterMillisecondSystem::IsBeforeStart(time_meter_comp));

	// 修改开始时间
	time_meter_comp.set_start(current_time_ms + 20000); // 设置为20秒后
	EXPECT_TRUE(TimeMeterMillisecondSystem::IsBeforeStart(time_meter_comp));
}

TEST_F(TimeMeterMillisecondSystemTest, IsNotStarted) {
	// 测试是否未开始
	EXPECT_FALSE(TimeMeterMillisecondSystem::IsNotStarted(time_meter_comp));

	// 修改开始时间
	time_meter_comp.set_start(current_time_ms + 20000); // 设置为20秒后
	EXPECT_TRUE(TimeMeterMillisecondSystem::IsNotStarted(time_meter_comp));
}

TEST_F(TimeMeterMillisecondSystemTest, Reset) {
	// 测试重置功能
	TimeMeterMillisecondSystem::Reset(time_meter_comp);
	uint64_t new_start = time_meter_comp.start();
	EXPECT_GE(new_start, current_time_ms);
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
