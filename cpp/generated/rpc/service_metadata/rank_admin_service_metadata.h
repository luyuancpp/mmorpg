#pragma once
#include <cstdint>

#include "proto/rank/rank_admin.pb.h"

constexpr uint32_t RankAdminOpenRankGenerationMessageId = 280;
constexpr uint32_t RankAdminOpenRankGenerationIndex = 0;
#define RankAdminOpenRankGenerationMethod  ::RankAdmin_Stub::descriptor()->method(0)

constexpr uint32_t RankAdminSubmitRankScoresMessageId = 278;
constexpr uint32_t RankAdminSubmitRankScoresIndex = 1;
#define RankAdminSubmitRankScoresMethod  ::RankAdmin_Stub::descriptor()->method(1)

constexpr uint32_t RankAdminFreezeRankGenerationMessageId = 294;
constexpr uint32_t RankAdminFreezeRankGenerationIndex = 2;
#define RankAdminFreezeRankGenerationMethod  ::RankAdmin_Stub::descriptor()->method(2)

constexpr uint32_t RankAdminRemoveRankEntryMessageId = 274;
constexpr uint32_t RankAdminRemoveRankEntryIndex = 3;
#define RankAdminRemoveRankEntryMethod  ::RankAdmin_Stub::descriptor()->method(3)

constexpr uint32_t RankAdminDeleteRankGenerationMessageId = 303;
constexpr uint32_t RankAdminDeleteRankGenerationIndex = 4;
#define RankAdminDeleteRankGenerationMethod  ::RankAdmin_Stub::descriptor()->method(4)
