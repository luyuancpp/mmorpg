
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/skillpermission_table.pb.h"

// ============================================================
// Per-column ECS components for SkillPermissionTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct SkillPermissionIdComp {
    uint32_t value;
};

struct SkillPermissionSkill_typeComp {
    std::span<const uint32_t> values;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline SkillPermissionIdComp MakeSkillPermissionIdComp(const SkillPermissionTable& row) {
    return { row.id() };
}
inline SkillPermissionSkill_typeComp MakeSkillPermissionSkill_typeComp(const SkillPermissionTable& row) {
    const auto& rf = row.skill_type();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
