#pragma once

#include <chrono>

#include "service_info/service_info.h"

#include "proto/common/statistics.pb.h"

extern std::array<MessageStatistics, kMaxMessageLen> g_message_statistics;

extern std::array<std::chrono::steady_clock::time_point, kMaxMessageLen> g_start_times;

extern uint64_t g_total_flow; // 所有消息的总流量