#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "time/util/time_meter_util.h"

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

	TimeMeterSecondUtil::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(6)); // �ȴ�6��

	EXPECT_TRUE(TimeMeterSecondUtil::IsExpired(timeMeter));
}

TEST_F(TimeMeterUtilTest, ExtendedDuration) {
	timeMeter.set_duration(5); // ����ʱ�����������ʱ��Ϊ5��

	TimeMeterSecondUtil::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(3)); // �ȴ�3��

	EXPECT_FALSE(TimeMeterSecondUtil::IsExpired(timeMeter));

	// ����ʱ��������ĳ���ʱ��Ϊ10��
	timeMeter.set_duration(10);

	TimeMeterSecondUtil::Reset(timeMeter);

	std::this_thread::sleep_for(std::chrono::seconds(6)); // �ȴ�6��

	EXPECT_FALSE(TimeMeterSecondUtil::IsExpired(timeMeter));
	EXPECT_EQ(TimeMeterSecondUtil::Remaining(timeMeter), 4); // 10�����ʱ���ȥ6�룬ʣ��4��
}


class TimeMeterMillisecondUtilTest : public ::testing::Test {
protected:
	void SetUp() override {
		// ����һ����׼ʱ��
		current_time_ms = TimeMeterMillisecondUtil::GetCurrentTimeInMilliseconds();
		time_meter_comp.set_start(current_time_ms);
		time_meter_comp.set_duration(10000); // 10��
	}

	uint64_t current_time_ms;
	TimeMeterComp time_meter_comp;
};

TEST_F(TimeMeterMillisecondUtilTest, RemainingTime) {
	// ����ʣ��ʱ��
	uint64_t remaining = TimeMeterMillisecondUtil::Remaining(time_meter_comp);
	EXPECT_LE(remaining, 10000);
}

TEST_F(TimeMeterMillisecondUtilTest, IsExpired) {
	// �����Ƿ�ʱ
	EXPECT_FALSE(TimeMeterMillisecondUtil::IsExpired(time_meter_comp));

	// �޸ĳ���ʱ����ʹ�䳬ʱ
	time_meter_comp.set_start(current_time_ms - 20000); // ��ʼʱ������Ϊ20��ǰ
	EXPECT_TRUE(TimeMeterMillisecondUtil::IsExpired(time_meter_comp));
}

TEST_F(TimeMeterMillisecondUtilTest, IsBeforeStart) {
	// �����Ƿ��ڿ�ʼʱ��֮ǰ
	EXPECT_FALSE(TimeMeterMillisecondUtil::IsBeforeStart(time_meter_comp));

	// �޸Ŀ�ʼʱ��
	time_meter_comp.set_start(current_time_ms + 20000); // ����Ϊ20���
	EXPECT_TRUE(TimeMeterMillisecondUtil::IsBeforeStart(time_meter_comp));
}

TEST_F(TimeMeterMillisecondUtilTest, IsNotStarted) {
	// �����Ƿ�δ��ʼ
	EXPECT_FALSE(TimeMeterMillisecondUtil::IsNotStarted(time_meter_comp));

	// �޸Ŀ�ʼʱ��
	time_meter_comp.set_start(current_time_ms + 20000); // ����Ϊ20���
	EXPECT_TRUE(TimeMeterMillisecondUtil::IsNotStarted(time_meter_comp));
}

TEST_F(TimeMeterMillisecondUtilTest, Reset) {
	// �������ù���
	TimeMeterMillisecondUtil::Reset(time_meter_comp);
	uint64_t new_start = time_meter_comp.start();
	EXPECT_GE(new_start, current_time_ms);
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
