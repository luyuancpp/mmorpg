
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/equipaffixrule_table.pb.h"

// ============================================================
// Per-column ECS components for EquipAffixRuleTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct EquipAffixRuleIdComp {
    uint32_t value;
};

struct EquipAffixRuleBlue_minComp {
    uint32_t value;
};

struct EquipAffixRuleBlue_maxComp {
    uint32_t value;
};

struct EquipAffixRulePink_rateComp {
    uint32_t value;
};

struct EquipAffixRuleYellow_rateComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline EquipAffixRuleIdComp MakeEquipAffixRuleIdComp(const EquipAffixRuleTable& row) {
    return { row.id() };
}
inline EquipAffixRuleBlue_minComp MakeEquipAffixRuleBlue_minComp(const EquipAffixRuleTable& row) {
    return { row.blue_min() };
}
inline EquipAffixRuleBlue_maxComp MakeEquipAffixRuleBlue_maxComp(const EquipAffixRuleTable& row) {
    return { row.blue_max() };
}
inline EquipAffixRulePink_rateComp MakeEquipAffixRulePink_rateComp(const EquipAffixRuleTable& row) {
    return { row.pink_rate() };
}
inline EquipAffixRuleYellow_rateComp MakeEquipAffixRuleYellow_rateComp(const EquipAffixRuleTable& row) {
    return { row.yellow_rate() };
}
