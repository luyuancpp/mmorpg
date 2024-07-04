#pragma once

#include "service/service.h"

#include "common_proto/statistics.pb.h"

extern std::array<MessageStatistics, kMaxMessageLen> g_message_statistics;