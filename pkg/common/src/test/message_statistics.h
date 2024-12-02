#pragma once
#include <iostream>
#include <unordered_map>
#include <cstdint>

#include "network/message_statistics.h"
#include "proto/common/common_message.pb.h"

#include "test/test.h"

// 功能函数：检查消息统计是否启用
bool IsMessageStatisticsEnabled() {
    return gFeatureSwitches[kTestMessageStatistics];
}

// 功能函数：更新消息统计计数
void IncrementMessageCount(int32_t message_id) {
    // 通过 message_id 查找对应的消息统计数据，如果不存在则创建新条目
    auto& statistic = g_message_statistics[message_id];
    statistic.set_count(statistic.count() + 1);
}

// 主逻辑函数：处理消息统计
void UpdateMessageStatistics(const MessageBody& message) {
    // 如果消息统计功能未启用，直接返回
    if (!IsMessageStatisticsEnabled()) {
        return;
    }

    // 获取消息ID并更新统计
    int32_t message_id = message.message_id();
    IncrementMessageCount(message_id);
}

