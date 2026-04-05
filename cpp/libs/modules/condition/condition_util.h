#pragma once

#include <cstdint>

#include <google/protobuf/repeated_field.h>

class ConditionTable;

/// General-purpose condition evaluation utilities.
/// Shared across mission, achievement, daily-login, and other systems
/// that reference the condition table.
namespace condition_util
{

    /// Checks whether progressValue satisfies the condition's comparison_op against targetCount.
    bool IsFulfilled(uint32_t conditionId, uint32_t progressValue, uint32_t targetCount);

    /// Checks whether all configured condition slots match the given event IDs.
    bool MatchesEventSlots(const ConditionTable *conditionRow,
                           const google::protobuf::RepeatedField<uint32_t> &eventConditionIds);

    /// Clamps progress to targetCount if the condition is fulfilled.
    uint32_t ClampIfFulfilled(uint32_t conditionId, uint32_t progress, uint32_t targetCount);

} // namespace condition_util
