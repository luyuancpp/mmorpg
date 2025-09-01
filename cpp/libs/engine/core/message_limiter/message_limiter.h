#pragma once

#include <cstdint>
#include <unordered_map>
#include <boost/circular_buffer.hpp>

class MessageLimiter {
public:
    MessageLimiter() {}

    MessageLimiter(uint8_t defaultMaxRequests, uint64_t defaultTimeWindow);

    uint32_t CanSend(uint32_t messageId);
    
private:
    uint8_t defaultMaxRequests{3};  // 最大允许次数
    uint64_t defaultTimeWindow{1};   // 时间窗口（秒）
    std::unordered_map<uint32_t, boost::circular_buffer<uint64_t>> requestRecords;
};

