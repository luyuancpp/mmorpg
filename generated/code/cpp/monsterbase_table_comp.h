
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/monsterbase_table.pb.h"

// ============================================================
// Per-column ECS components for MonsterBaseTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct MonsterBaseIdComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline MonsterBaseIdComp MakeMonsterBaseIdComp(const MonsterBaseTable& row) {
    return { row.id() };
}
