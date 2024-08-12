#pragma once

#include "service_info/service_info.h"

#include "proto/common/statistics.pb.h"

extern std::array<MessageStatistics, kMaxMessageLen> g_message_statistics;