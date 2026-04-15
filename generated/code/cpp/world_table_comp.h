
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/world_table.pb.h"

// ============================================================
// Per-column ECS components for WorldTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct WorldIdComp {
    uint32_t value;
};

struct WorldScene_idComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline WorldIdComp MakeWorldIdComp(const WorldTable& row) {
    return { row.id() };
}
inline WorldScene_idComp MakeWorldScene_idComp(const WorldTable& row) {
    return { row.scene_id() };
}
