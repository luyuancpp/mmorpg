
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/condition_table.pb.h"

// ============================================================
// Per-column ECS components for ConditionTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct ConditionIdComp {
    uint32_t value;
};

struct ConditionCondition_categoryComp {
    uint32_t value;
};

struct ConditionValid_durationComp {
    uint64_t value;
};

struct ConditionQuantity_typeComp {
    uint32_t value;
};

struct ConditionTarget_countComp {
    uint32_t value;
};

struct ConditionComparison_opComp {
    uint32_t value;
};

struct ConditionCondition1Comp {
    std::span<const uint32_t> values;
};

struct ConditionCondition2Comp {
    std::span<const uint32_t> values;
};

struct ConditionCondition3Comp {
    std::span<const uint32_t> values;
};

struct ConditionCondition4Comp {
    std::span<const uint32_t> values;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline ConditionIdComp MakeConditionIdComp(const ConditionTable& row) {
    return { row.id() };
}
inline ConditionCondition_categoryComp MakeConditionCondition_categoryComp(const ConditionTable& row) {
    return { row.condition_category() };
}
inline ConditionValid_durationComp MakeConditionValid_durationComp(const ConditionTable& row) {
    return { row.valid_duration() };
}
inline ConditionQuantity_typeComp MakeConditionQuantity_typeComp(const ConditionTable& row) {
    return { row.quantity_type() };
}
inline ConditionTarget_countComp MakeConditionTarget_countComp(const ConditionTable& row) {
    return { row.target_count() };
}
inline ConditionComparison_opComp MakeConditionComparison_opComp(const ConditionTable& row) {
    return { row.comparison_op() };
}
inline ConditionCondition1Comp MakeConditionCondition1Comp(const ConditionTable& row) {
    const auto& rf = row.condition1();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
inline ConditionCondition2Comp MakeConditionCondition2Comp(const ConditionTable& row) {
    const auto& rf = row.condition2();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
inline ConditionCondition3Comp MakeConditionCondition3Comp(const ConditionTable& row) {
    const auto& rf = row.condition3();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
inline ConditionCondition4Comp MakeConditionCondition4Comp(const ConditionTable& row) {
    const auto& rf = row.condition4();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
