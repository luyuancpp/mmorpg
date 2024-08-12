#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "time/comp/time_meter_util.h"

class TimeMeterUtilTest : public ::testing::Test {
protected:
	TimeMeterComp timeMeter;

	void SetUp() override {
		// ��ʼ�����루�����Ҫ��
	}

	void TearDown() override {
		// ������루�����Ҫ��
	}
};

TEST_F(TimeMeterUtilTest, InitialExpiration) {
	timeMeter.set_duration(5); // ����ʱ�����������ʱ��Ϊ5��

	TimeMeterUtil::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(6)); // �ȴ�6��

	EXPECT_TRUE(TimeMeterUtil::IsExpired(timeMeter));
}

TEST_F(TimeMeterUtilTest, ExtendedDuration) {
	timeMeter.set_duration(5); // ����ʱ�����������ʱ��Ϊ5��

	TimeMeterUtil::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(3)); // �ȴ�3��

	EXPECT_FALSE(TimeMeterUtil::IsExpired(timeMeter));

	// ����ʱ��������ĳ���ʱ��Ϊ10��
	timeMeter.set_duration(10);

	TimeMeterUtil::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(6)); // �ȴ�6��

	EXPECT_FALSE(TimeMeterUtil::IsExpired(timeMeter));
	EXPECT_EQ(TimeMeterUtil::Remaining(timeMeter), 4); // 10�����ʱ���ȥ6�룬ʣ��4��
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
