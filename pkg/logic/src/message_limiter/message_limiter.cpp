#include "message_limiter.h"
#include <chrono>

MessageLimiter::MessageLimiter(int maxRequests, int timeWindow)
    : maxRequests(maxRequests), timeWindow(timeWindow) {}

bool MessageLimiter::canSend(uint32_t messageId) {
    auto currentTime = getCurrentTime();
    auto& timestamps = records[messageId];

    // 清除过期时间戳
    while (!timestamps.empty() && currentTime - timestamps.front() > timeWindow) {
        timestamps.pop_front();
    }

    if (timestamps.size() < maxRequests) {
        timestamps.push_back(currentTime);
        return true;
    }
    else {
        return false;
    }
}

long long MessageLimiter::getCurrentTime() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}
