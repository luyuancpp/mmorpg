
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/class_table.pb.h"

// ============================================================
// Per-column ECS components for ClassTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct ClassIdComp {
    uint32_t value;
};

struct ClassSkillComp {
    std::span<const uint32_t> values;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline ClassIdComp MakeClassIdComp(const ClassTable& row) {
    return { row.id() };
}
inline ClassSkillComp MakeClassSkillComp(const ClassTable& row) {
    const auto& rf = row.skill();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
