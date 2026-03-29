
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/monster_table.pb.h"

// ============================================================
// Per-column ECS components for MonsterTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct MonsterIdComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline MonsterIdComp MakeMonsterIdComp(const MonsterTable& row) {
    return { row.id() };
}
