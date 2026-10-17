#pragma once
#include <cstdint>

#include "proto/rank/rank.pb.h"

constexpr uint32_t ClientPlayerRankGetRankPageMessageId = 292;
constexpr uint32_t ClientPlayerRankGetRankPageIndex = 0;
#define ClientPlayerRankGetRankPageMethod  ::ClientPlayerRank_Stub::descriptor()->method(0)

constexpr uint32_t ClientPlayerRankGetRankAroundMessageId = 272;
constexpr uint32_t ClientPlayerRankGetRankAroundIndex = 1;
#define ClientPlayerRankGetRankAroundMethod  ::ClientPlayerRank_Stub::descriptor()->method(1)
