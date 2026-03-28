
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/reward_table.pb.h"

// ============================================================
// Per-column ECS components for RewardTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct RewardIdComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline RewardIdComp MakeRewardIdComp(const RewardTable& row) {
    return { row.id() };
}
