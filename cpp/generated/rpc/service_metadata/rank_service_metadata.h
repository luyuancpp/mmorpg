#pragma once
#include <cstdint>

#include "proto/rank/rank.pb.h"

constexpr uint32_t ClientPlayerRankGetRankPageMessageId = 275;
constexpr uint32_t ClientPlayerRankGetRankPageIndex = 0;
#define ClientPlayerRankGetRankPageMethod  ::ClientPlayerRank_Stub::descriptor()->method(0)
