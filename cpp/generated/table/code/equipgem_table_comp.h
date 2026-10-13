
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/equipgem_table.pb.h"

// ============================================================
// Per-column ECS components for EquipGemTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct EquipGemIdComp {
    uint32_t value;
};

struct EquipGemGem_typeComp {
    uint32_t value;
};

struct EquipGemAttr_idComp {
    uint32_t value;
};

struct EquipGemValueComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline EquipGemIdComp MakeEquipGemIdComp(const EquipGemTable& row) {
    return { row.id() };
}
inline EquipGemGem_typeComp MakeEquipGemGem_typeComp(const EquipGemTable& row) {
    return { row.gem_type() };
}
inline EquipGemAttr_idComp MakeEquipGemAttr_idComp(const EquipGemTable& row) {
    return { row.attr_id() };
}
inline EquipGemValueComp MakeEquipGemValueComp(const EquipGemTable& row) {
    return { row.value() };
}
