#pragma once

#include "service/service.h"

#include "proto/common/statistics.pb.h"

extern std::array<MessageStatistics, kMaxMessageLen> g_message_statistics;