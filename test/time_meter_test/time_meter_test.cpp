#include <iostream>
#include <chrono>
#include <thread> // Ϊ����ʾ�������߳̿���ģ��ʱ������
#include <gtest/gtest.h>

#include "time/comp/time_meter_util.h"

TEST(TimeMeterUtil, IsExpired)
{
	TimeMeterComp timeMeter;
	timeMeter.set_duration(5); // ����ʱ�����������ʱ��Ϊ5��

	// ��ʼ��ʱ
	TimeMeterUtil::Reset(timeMeter);

	// ģ��һЩ��ʱ�Ĳ���������ʱ�䲻����5��
	for (int i = 0; i < 10; ++i) {
		std::this_thread::sleep_for(std::chrono::seconds(1)); // ģ���ʱ1��Ĳ���
		std::cout << "Time remaining: " << TimeMeterUtil::Remaining(timeMeter) << " seconds\n";
		if (TimeMeterUtil::IsExpired(timeMeter)) {
			std::cout << "TimeMeter expired!\n";
			break;
		}
	}

	// ����ʱ��������ĳ���ʱ��Ϊ10��
	timeMeter.set_duration(10);

	// ���¿�ʼ��ʱ
	TimeMeterUtil::Reset(timeMeter);

	// �ٴ�ģ��һЩ��ʱ�Ĳ���������ʱ�䲻����10��
	for (int i = 0; i < 20; ++i) {
		std::this_thread::sleep_for(std::chrono::seconds(1)); // ģ���ʱ1��Ĳ���
		std::cout << "Time remaining: " << TimeMeterUtil::Remaining(timeMeter) << " seconds\n";
		if (TimeMeterUtil::IsExpired(timeMeter)) {
			std::cout << "TimeMeter expired!\n";
			break;
		}
	}
}

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

