#include <iostream>
#include <chrono>
#include <thread> // Ϊ����ʾ�������߳̿���ģ��ʱ������
#include <gtest/gtest.h>

#include "time/comp/time_meter.h"

TEST(TimeMeter, IsExpired)
{
	TimeMeter timeMeter;
	timeMeter.SetDuration(5); // ����ʱ�����������ʱ��Ϊ5��

	// ��ʼ��ʱ
	timeMeter.Reset();

	// ģ��һЩ��ʱ�Ĳ���������ʱ�䲻����5��
	for (int i = 0; i < 10; ++i) {
		std::this_thread::sleep_for(std::chrono::seconds(1)); // ģ���ʱ1��Ĳ���
		std::cout << "Time remaining: " << timeMeter.Remaining() << " seconds\n";
		if (timeMeter.IsExpired()) {
			std::cout << "TimeMeter expired!\n";
			break;
		}
	}

	// ����ʱ��������ĳ���ʱ��Ϊ10��
	timeMeter.SetDuration(10);

	// ���¿�ʼ��ʱ
	timeMeter.Reset();

	// �ٴ�ģ��һЩ��ʱ�Ĳ���������ʱ�䲻����10��
	for (int i = 0; i < 20; ++i) {
		std::this_thread::sleep_for(std::chrono::seconds(1)); // ģ���ʱ1��Ĳ���
		std::cout << "Time remaining: " << timeMeter.Remaining() << " seconds\n";
		if (timeMeter.IsExpired()) {
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

