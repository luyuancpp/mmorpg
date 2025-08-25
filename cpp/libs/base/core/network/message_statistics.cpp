#include "message_statistics.h"

std::array<MessageStatistics, kMaxMessageLen> gMessageStatistics;

std::array<std::chrono::steady_clock::time_point, kMaxMessageLen> gStartTimes;

uint64_t gTotalFlow = 0; // 所有消息的总流量