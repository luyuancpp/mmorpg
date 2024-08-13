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

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
