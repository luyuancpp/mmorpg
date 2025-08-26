#include "message_limiter.h"

#include "proto/table/common_error_tip.pb.h"
#include "table/messagelimiter_config.h"
#include "time/system/time_system.h"

// 构造函数，初始化默认的最大请求数和时间窗口
MessageLimiter::MessageLimiter(uint8_t defaultMaxRequests, uint64_t defaultTimeWindow)
    : defaultMaxRequests(defaultMaxRequests), defaultTimeWindow(defaultTimeWindow) {}

uint32_t MessageLimiter::CanSend(uint32_t messageId) {
    // 获取当前时间（毫秒）
    const auto currentTime = TimeUtil::NowSeconds();

    // 获取当前消息ID的时间戳记录
    auto& messageTimestamps = requestRecords[messageId];

    // 默认限流规则
    uint8_t maxAllowedRequests = this->defaultMaxRequests;
    uint64_t timeWindowDuration = this->defaultTimeWindow;

    // 检查是否有针对该消息ID的特殊配置
    if (const auto [configEntry, fetchSuccess] = MessageLimiterConfigurationTable::Instance().GetTableWithoutErrorLogging(messageId);
        configEntry != nullptr) {
        maxAllowedRequests = configEntry->maxrequests();   // 覆盖最大请求数
        timeWindowDuration = configEntry->timewindow(); // 覆盖时间窗口
    }

    // 初始化时间戳存储
    if (messageTimestamps.empty()) {
        messageTimestamps.resize(maxAllowedRequests);
    }

    // 清理过期的时间戳（超出时间窗口的时间戳）
    while (!messageTimestamps.empty() && currentTime - messageTimestamps.front() > timeWindowDuration) {
        messageTimestamps.pop_front();
    }

    // 判断是否允许发送消息
    if (messageTimestamps.size() < maxAllowedRequests) {
        messageTimestamps.push_back(currentTime); // 记录当前时间戳
        return kSuccess; // 允许请求
    }
    else {
        return kRateLimitExceeded; // 超出限制，拒绝请求,超出限流
    }
}
