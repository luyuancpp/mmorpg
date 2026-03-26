#pragma once

#include <cstdint>

class ConditionTable;
class MissionConditionEvent;

/// General-purpose condition evaluation utilities.
/// Shared across mission, achievement, daily-login, and other systems
/// that reference the condition table.
namespace condition_util {

bool IsFulfilled(uint32_t conditionId, uint32_t progressValue);

bool MatchesEventSlots(const ConditionTable* conditionRow,
                       const MissionConditionEvent& conditionEvent);

uint32_t ClampIfFulfilled(uint32_t conditionId, uint32_t progress);

} // namespace condition_util
