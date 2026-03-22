#include "message_statistics.h"

std::array<MessageStatistics, kMaxRpcMethodCount> gMessageStatistics;

std::array<std::chrono::steady_clock::time_point, kMaxRpcMethodCount> gStartTimes;

uint64_t gTotalFlow = 0; // Total flow across all messages