#pragma once

#include <chrono>

#include "rpc/service_metadata/rpc_event_registry.h"

#include "proto/common/base/statistics.pb.h"

extern std::array<MessageStatistics, kMaxRpcMethodCount> gMessageStatistics;

extern std::array<std::chrono::steady_clock::time_point, kMaxRpcMethodCount> gStartTimes;

extern uint64_t gTotalFlow; // Total flow across all messages