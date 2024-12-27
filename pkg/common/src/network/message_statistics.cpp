#include "message_statistics.h"

std::array<MessageStatistics, kMaxMessageLen> g_message_statistics;

std::array<std::chrono::steady_clock::time_point, kMaxMessageLen> g_start_times;

uint64_t g_total_flow = 0; // 所有消息的总流量