#ifndef LIMITER_H
#define LIMITER_H

#include <unordered_map>
#include <deque>
#include <cstdint>

class MessageLimiter {
public:
    MessageLimiter(int maxRequests, int timeWindow);
    bool canSend(uint32_t messageId);

private:
    int maxRequests;  // 最大允许次数
    int timeWindow;   // 时间窗口（秒）
    std::unordered_map<uint32_t, std::deque<long long>> records;

    long long getCurrentTime();
};

#endif // LIMITER_H
