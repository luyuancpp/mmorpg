#pragma once

#include <chrono>

#include "service_info/service_info.h"

#include "proto/common/statistics.pb.h"

extern std::array<MessageStatistics, kMaxMessageLen> gMessageStatistics;

extern std::array<std::chrono::steady_clock::time_point, kMaxMessageLen> gStartTimes;

extern uint64_t gTotalFlow; // 所有消息的总流量