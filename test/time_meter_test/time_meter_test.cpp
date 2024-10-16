#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "time/util/time_meter_util.h"

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

	TimeMeterSecondUtil::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(6)); // 等待6秒

	EXPECT_TRUE(TimeMeterSecondUtil::IsExpired(timeMeter));
}

TEST_F(TimeMeterUtilTest, ExtendedDuration) {
	timeMeter.set_duration(5); // 设置时间测量器持续时间为5秒

	TimeMeterSecondUtil::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(3)); // 等待3秒

	EXPECT_FALSE(TimeMeterSecondUtil::IsExpired(timeMeter));

	// 调整时间测量器的持续时间为10秒
	timeMeter.set_duration(10);

	TimeMeterSecondUtil::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(6)); // 等待6秒

	EXPECT_FALSE(TimeMeterSecondUtil::IsExpired(timeMeter));
	EXPECT_EQ(TimeMeterSecondUtil::Remaining(timeMeter), 4); // 10秒持续时间减去6秒，剩余4秒
}


class TimeMeterMillisecondUtilTest : public ::testing::Test {
protected:
	void SetUp() override {
		// 设置一个标准时间
		current_time_ms = TimeMeterMillisecondUtil::GetCurrentTimeInMilliseconds();
		time_meter_comp.set_start(current_time_ms);
		time_meter_comp.set_duration(10000); // 10秒
	}

	uint64_t current_time_ms;
	TimeMeterComp time_meter_comp;
};

TEST_F(TimeMeterMillisecondUtilTest, RemainingTime) {
	// 测试剩余时间
	uint64_t remaining = TimeMeterMillisecondUtil::Remaining(time_meter_comp);
	EXPECT_LE(remaining, 10000);
}

TEST_F(TimeMeterMillisecondUtilTest, IsExpired) {
	// 测试是否超时
	EXPECT_FALSE(TimeMeterMillisecondUtil::IsExpired(time_meter_comp));

	// 修改持续时间以使其超时
	time_meter_comp.set_start(current_time_ms - 20000); // 起始时间设置为20秒前
	EXPECT_TRUE(TimeMeterMillisecondUtil::IsExpired(time_meter_comp));
}

TEST_F(TimeMeterMillisecondUtilTest, IsBeforeStart) {
	// 测试是否在开始时间之前
	EXPECT_FALSE(TimeMeterMillisecondUtil::IsBeforeStart(time_meter_comp));

	// 修改开始时间
	time_meter_comp.set_start(current_time_ms + 20000); // 设置为20秒后
	EXPECT_TRUE(TimeMeterMillisecondUtil::IsBeforeStart(time_meter_comp));
}

TEST_F(TimeMeterMillisecondUtilTest, IsNotStarted) {
	// 测试是否未开始
	EXPECT_FALSE(TimeMeterMillisecondUtil::IsNotStarted(time_meter_comp));

	// 修改开始时间
	time_meter_comp.set_start(current_time_ms + 20000); // 设置为20秒后
	EXPECT_TRUE(TimeMeterMillisecondUtil::IsNotStarted(time_meter_comp));
}

TEST_F(TimeMeterMillisecondUtilTest, Reset) {
	// 测试重置功能
	TimeMeterMillisecondUtil::Reset(time_meter_comp);
	uint64_t new_start = time_meter_comp.start();
	EXPECT_GE(new_start, current_time_ms);
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
