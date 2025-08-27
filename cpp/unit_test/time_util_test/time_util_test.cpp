#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "time/system/time_system.h"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "muduo/base/CrossPlatformAdapterFunction.h"

void PrintUTCWithMilliseconds() {
	uint64_t milliseconds = TimeUtil::NowMillisecondsUTC();
	boost::posix_time::ptime utc_now = boost::posix_time::microsec_clock::universal_time();

	std::cout << "当前 UTC 时间: " << boost::posix_time::to_simple_string(utc_now) << std::endl;
	std::cout << "自1970年1月1日以来的毫秒数: " << milliseconds << std::endl;
}

void PrintUTCWithSeconds() {
	uint64_t seconds = TimeUtil::NowSecondsUTC();
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();

	// 输出 UTC 时间
	std::cout << "当前 UTC 时间: " << boost::posix_time::to_simple_string(utc_now) << std::endl;
	std::cout << "自1970年1月1日以来的秒数: " << seconds << std::endl;
}

TEST(TimeUtilTest, NowMilliseconds) {
	uint64_t localMilliseconds = TimeUtil::NowMilliseconds();
	EXPECT_GE(localMilliseconds, 0);  // 检查返回值是否大于或等于 0
}

TEST(TimeUtilTest, NowSeconds) {
	uint64_t localSeconds = TimeUtil::NowSeconds();
	EXPECT_GE(localSeconds, 0);  // 检查返回值是否大于或等于 0
}

TEST(TimeUtilTest, NowMillisecondsUTC) {
	uint64_t utcMilliseconds = TimeUtil::NowMillisecondsUTC();
	EXPECT_GE(utcMilliseconds, 0);  // 检查返回值是否大于或等于 0
}

TEST(TimeUtilTest, NowSecondsUTC) {
	uint64_t utcSeconds = TimeUtil::NowSecondsUTC();
	EXPECT_GE(utcSeconds, 0);  // 检查返回值是否大于或等于 0
}

// 比较本地时间与 UTC 时间差异
TEST(TimeUtilTest, CompareLocalAndUTC) {
	uint64_t localMilliseconds = TimeUtil::NowMilliseconds();
	uint64_t utcMilliseconds = TimeUtil::NowMillisecondsUTC();

	// 本地时间与 UTC 时间的差值（应接近于 8 小时）
	int64_t difference = static_cast<int64_t>(localMilliseconds) - static_cast<int64_t>(utcMilliseconds);

	// 8 小时的毫秒数
	const uint64_t eightHoursInMilliseconds = 8 * 60 * 60 * 1000;

	EXPECT_LE(difference, eightHoursInMilliseconds);  // 本地时间应小于等于 UTC + 8 小时
	EXPECT_GE(difference, 0);  // 本地时间应大于或等于 UTC 时间
}

// 比较本地时间与 UTC 时间差异（以秒为单位）
TEST(TimeUtilTest, CompareLocalAndUTCInSeconds) {
	uint64_t localSeconds = TimeUtil::NowSeconds();
	uint64_t utcSeconds = TimeUtil::NowSecondsUTC();

	// 本地时间与 UTC 时间的差值（应接近于 8 小时）
	int64_t difference = static_cast<int64_t>(localSeconds) - static_cast<int64_t>(utcSeconds);

	// 8 小时的秒数
	const uint64_t eightHoursInSeconds = 8 * 60 * 60;

	EXPECT_LE(difference, eightHoursInSeconds);  // 本地时间应小于等于 UTC + 8 小时
	EXPECT_GE(difference, 0);  // 本地时间应大于或等于 UTC 时间
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}