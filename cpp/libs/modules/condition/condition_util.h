#pragma once

#include <cstdint>

#include <google/protobuf/repeated_field.h>

class ConditionTable;

/// General-purpose condition evaluation utilities.
/// Shared across mission, achievement, daily-login, and other systems
/// that reference the condition table.
namespace condition_util {

/// Uses target_count from condition table.
bool IsFulfilled(uint32_t conditionId, uint32_t progressValue);

/// Uses caller-supplied targetCount (for business systems that own their own target_count).
bool IsFulfilled(uint32_t conditionId, uint32_t progressValue, uint32_t targetCount);

/// Checks whether all configured condition slots match the given event IDs.
bool MatchesEventSlots(const ConditionTable* conditionRow,
                       const google::protobuf::RepeatedField<uint32_t>& eventConditionIds);

/// Uses target_count from condition table.
uint32_t ClampIfFulfilled(uint32_t conditionId, uint32_t progress);

/// Uses caller-supplied targetCount.
uint32_t ClampIfFulfilled(uint32_t conditionId, uint32_t progress, uint32_t targetCount);

} // namespace condition_util
