
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/equipgemsocket_table.pb.h"

// ============================================================
// Per-column ECS components for EquipGemSocketTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct EquipGemSocketIdComp {
    uint32_t value;
};

struct EquipGemSocketEquip_kindComp {
    uint32_t value;
};

struct EquipGemSocketSocketComp {
    uint32_t value;
};

struct EquipGemSocketGem_typeComp {
    uint32_t value;
};

struct EquipGemSocketNameComp {
    std::string_view value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline EquipGemSocketIdComp MakeEquipGemSocketIdComp(const EquipGemSocketTable& row) {
    return { row.id() };
}
inline EquipGemSocketEquip_kindComp MakeEquipGemSocketEquip_kindComp(const EquipGemSocketTable& row) {
    return { row.equip_kind() };
}
inline EquipGemSocketSocketComp MakeEquipGemSocketSocketComp(const EquipGemSocketTable& row) {
    return { row.socket() };
}
inline EquipGemSocketGem_typeComp MakeEquipGemSocketGem_typeComp(const EquipGemSocketTable& row) {
    return { row.gem_type() };
}
inline EquipGemSocketNameComp MakeEquipGemSocketNameComp(const EquipGemSocketTable& row) {
    return { std::string_view(row.name()) };
}
