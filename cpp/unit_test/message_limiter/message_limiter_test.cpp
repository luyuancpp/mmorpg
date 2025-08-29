#include "message_limiter/message_limiter.h"
#include <chrono>
#include <thread>
#include <gtest/gtest.h>
#include "proto/table/tip/common_error_tip.pb.h"

TEST(MessageLimiterTest, AllowInitialRequests) {
    MessageLimiter limiter(5, 60);  // 每 60 秒允许 5 次
    uint32_t messageId = 12345;

    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(kSuccess, limiter.CanSend(messageId)) << "第 " << i + 1 << " 次发送应该被允许";
    }

    EXPECT_EQ(kRateLimitExceeded, limiter.CanSend(messageId)) << "第 6 次发送应该被拒绝";
}

TEST(MessageLimiterTest, ClearExpiredRequests) {
    MessageLimiter limiter(2, 2);  // 每 2 秒允许 2 次
    uint32_t messageId = 67890;

    EXPECT_EQ(kSuccess, limiter.CanSend(messageId));
    EXPECT_EQ(kSuccess, limiter.CanSend(messageId));
    EXPECT_EQ(kRateLimitExceeded, limiter.CanSend(messageId)) << "超过限制，应被拒绝";

    // 等待 3 秒，让限制窗口过期
    std::this_thread::sleep_for(std::chrono::seconds(3));

    EXPECT_EQ(kSuccess, limiter.CanSend(messageId)) << "时间窗口过期后，应被允许";
}

TEST(MessageLimiterTest, DifferentMessageIDs) {
    MessageLimiter limiter(3, 60);  // 每 60 秒允许 3 次
    uint32_t messageId1 = 11111;
    uint32_t messageId2 = 22222;

    EXPECT_EQ(kSuccess, limiter.CanSend(messageId1));
    EXPECT_EQ(kSuccess, limiter.CanSend(messageId2));
    EXPECT_EQ(kSuccess, limiter.CanSend(messageId1));
    EXPECT_EQ(kSuccess, limiter.CanSend(messageId2));
    EXPECT_EQ(kSuccess, limiter.CanSend(messageId1));

    EXPECT_EQ(kRateLimitExceeded, limiter.CanSend(messageId1)) << "messageId1 超过限制，应被拒绝";
    EXPECT_EQ(kSuccess, limiter.CanSend(messageId2)) << "messageId2 未超过限制，应被允许";
}
