#pragma once
#include <cstdint>

#include "proto/match/match_internal.pb.h"

constexpr uint32_t MatchInternalStartActivityBattleMessageId = 245;
constexpr uint32_t MatchInternalStartActivityBattleIndex = 0;
#define MatchInternalStartActivityBattleMethod  ::MatchInternal_Stub::descriptor()->method(0)
