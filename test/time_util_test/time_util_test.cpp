#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "time/util/time_system.h"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "muduo/base/CrossPlatformAdapterFunction.h"

void PrintUTCWithMilliseconds() {
	uint64_t milliseconds = TimeUtil::NowMillisecondsUTC();
	boost::posix_time::ptime utc_now = boost::posix_time::microsec_clock::universal_time();

	std::cout << "��ǰ UTC ʱ��: " << boost::posix_time::to_simple_string(utc_now) << std::endl;
	std::cout << "��1970��1��1�������ĺ�����: " << milliseconds << std::endl;
}

void PrintUTCWithSeconds() {
	uint64_t seconds = TimeUtil::NowSecondsUTC();
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();

	// ��� UTC ʱ��
	std::cout << "��ǰ UTC ʱ��: " << boost::posix_time::to_simple_string(utc_now) << std::endl;
	std::cout << "��1970��1��1������������: " << seconds << std::endl;
}

TEST(TimeUtilTest, NowMilliseconds) {
	uint64_t localMilliseconds = TimeUtil::NowMilliseconds();
	EXPECT_GE(localMilliseconds, 0);  // ��鷵��ֵ�Ƿ���ڻ���� 0
}

TEST(TimeUtilTest, NowSeconds) {
	uint64_t localSeconds = TimeUtil::NowSeconds();
	EXPECT_GE(localSeconds, 0);  // ��鷵��ֵ�Ƿ���ڻ���� 0
}

TEST(TimeUtilTest, NowMillisecondsUTC) {
	uint64_t utcMilliseconds = TimeUtil::NowMillisecondsUTC();
	EXPECT_GE(utcMilliseconds, 0);  // ��鷵��ֵ�Ƿ���ڻ���� 0
}

TEST(TimeUtilTest, NowSecondsUTC) {
	uint64_t utcSeconds = TimeUtil::NowSecondsUTC();
	EXPECT_GE(utcSeconds, 0);  // ��鷵��ֵ�Ƿ���ڻ���� 0
}

// �Ƚϱ���ʱ���� UTC ʱ�����
TEST(TimeUtilTest, CompareLocalAndUTC) {
	uint64_t localMilliseconds = TimeUtil::NowMilliseconds();
	uint64_t utcMilliseconds = TimeUtil::NowMillisecondsUTC();

	// ����ʱ���� UTC ʱ��Ĳ�ֵ��Ӧ�ӽ��� 8 Сʱ��
	int64_t difference = static_cast<int64_t>(localMilliseconds) - static_cast<int64_t>(utcMilliseconds);

	// 8 Сʱ�ĺ�����
	const uint64_t eightHoursInMilliseconds = 8 * 60 * 60 * 1000;

	EXPECT_LE(difference, eightHoursInMilliseconds);  // ����ʱ��ӦС�ڵ��� UTC + 8 Сʱ
	EXPECT_GE(difference, 0);  // ����ʱ��Ӧ���ڻ���� UTC ʱ��
}

// �Ƚϱ���ʱ���� UTC ʱ����죨����Ϊ��λ��
TEST(TimeUtilTest, CompareLocalAndUTCInSeconds) {
	uint64_t localSeconds = TimeUtil::NowSeconds();
	uint64_t utcSeconds = TimeUtil::NowSecondsUTC();

	// ����ʱ���� UTC ʱ��Ĳ�ֵ��Ӧ�ӽ��� 8 Сʱ��
	int64_t difference = static_cast<int64_t>(localSeconds) - static_cast<int64_t>(utcSeconds);

	// 8 Сʱ������
	const uint64_t eightHoursInSeconds = 8 * 60 * 60;

	EXPECT_LE(difference, eightHoursInSeconds);  // ����ʱ��ӦС�ڵ��� UTC + 8 Сʱ
	EXPECT_GE(difference, 0);  // ����ʱ��Ӧ���ڻ���� UTC ʱ��
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}