
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/equipaffixpool_table.pb.h"

// ============================================================
// Per-column ECS components for EquipAffixPoolTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct EquipAffixPoolIdComp {
    uint32_t value;
};

struct EquipAffixPoolPool_idComp {
    uint32_t value;
};

struct EquipAffixPoolAttr_idComp {
    uint32_t value;
};

struct EquipAffixPoolWeightComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline EquipAffixPoolIdComp MakeEquipAffixPoolIdComp(const EquipAffixPoolTable& row) {
    return { row.id() };
}
inline EquipAffixPoolPool_idComp MakeEquipAffixPoolPool_idComp(const EquipAffixPoolTable& row) {
    return { row.pool_id() };
}
inline EquipAffixPoolAttr_idComp MakeEquipAffixPoolAttr_idComp(const EquipAffixPoolTable& row) {
    return { row.attr_id() };
}
inline EquipAffixPoolWeightComp MakeEquipAffixPoolWeightComp(const EquipAffixPoolTable& row) {
    return { row.weight() };
}
