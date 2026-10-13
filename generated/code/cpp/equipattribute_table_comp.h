
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/equipattribute_table.pb.h"

// ============================================================
// Per-column ECS components for EquipAttributeTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct EquipAttributeIdComp {
    uint32_t value;
};

struct EquipAttributeNameComp {
    std::string_view value;
};

struct EquipAttributeEffectComp {
    uint32_t value;
};

struct EquipAttributeEffect_paramComp {
    uint32_t value;
};

struct EquipAttributePercentComp {
    uint32_t value;
};

struct EquipAttributeSortComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline EquipAttributeIdComp MakeEquipAttributeIdComp(const EquipAttributeTable& row) {
    return { row.id() };
}
inline EquipAttributeNameComp MakeEquipAttributeNameComp(const EquipAttributeTable& row) {
    return { std::string_view(row.name()) };
}
inline EquipAttributeEffectComp MakeEquipAttributeEffectComp(const EquipAttributeTable& row) {
    return { row.effect() };
}
inline EquipAttributeEffect_paramComp MakeEquipAttributeEffect_paramComp(const EquipAttributeTable& row) {
    return { row.effect_param() };
}
inline EquipAttributePercentComp MakeEquipAttributePercentComp(const EquipAttributeTable& row) {
    return { row.percent() };
}
inline EquipAttributeSortComp MakeEquipAttributeSortComp(const EquipAttributeTable& row) {
    return { row.sort() };
}
