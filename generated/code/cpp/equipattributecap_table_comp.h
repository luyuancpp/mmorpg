
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/equipattributecap_table.pb.h"

// ============================================================
// Per-column ECS components for EquipAttributeCapTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct EquipAttributeCapIdComp {
    uint32_t value;
};

struct EquipAttributeCapAttr_idComp {
    uint32_t value;
};

struct EquipAttributeCapLevelComp {
    uint32_t value;
};

struct EquipAttributeCapMin_valueComp {
    uint32_t value;
};

struct EquipAttributeCapCapComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline EquipAttributeCapIdComp MakeEquipAttributeCapIdComp(const EquipAttributeCapTable& row) {
    return { row.id() };
}
inline EquipAttributeCapAttr_idComp MakeEquipAttributeCapAttr_idComp(const EquipAttributeCapTable& row) {
    return { row.attr_id() };
}
inline EquipAttributeCapLevelComp MakeEquipAttributeCapLevelComp(const EquipAttributeCapTable& row) {
    return { row.level() };
}
inline EquipAttributeCapMin_valueComp MakeEquipAttributeCapMin_valueComp(const EquipAttributeCapTable& row) {
    return { row.min_value() };
}
inline EquipAttributeCapCapComp MakeEquipAttributeCapCapComp(const EquipAttributeCapTable& row) {
    return { row.cap() };
}
