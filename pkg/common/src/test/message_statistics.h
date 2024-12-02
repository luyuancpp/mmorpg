#pragma once
#include <iostream>
#include <unordered_map>
#include <cstdint>

#include "network/message_statistics.h"
#include "proto/common/common_message.pb.h"

#include "test/test.h"

// ���ܺ����������Ϣͳ���Ƿ�����
bool IsMessageStatisticsEnabled() {
    return gFeatureSwitches[kTestMessageStatistics];
}

// ���ܺ�����������Ϣͳ�Ƽ���
void IncrementMessageCount(int32_t message_id) {
    // ͨ�� message_id ���Ҷ�Ӧ����Ϣͳ�����ݣ�����������򴴽�����Ŀ
    auto& statistic = g_message_statistics[message_id];
    statistic.set_count(statistic.count() + 1);
}

// ���߼�������������Ϣͳ��
void UpdateMessageStatistics(const MessageBody& message) {
    // �����Ϣͳ�ƹ���δ���ã�ֱ�ӷ���
    if (!IsMessageStatisticsEnabled()) {
        return;
    }

    // ��ȡ��ϢID������ͳ��
    int32_t message_id = message.message_id();
    IncrementMessageCount(message_id);
}

