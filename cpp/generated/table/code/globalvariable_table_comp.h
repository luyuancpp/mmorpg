
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/globalvariable_table.pb.h"

// ============================================================
// Per-column ECS components for GlobalVariableTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct GlobalVariableIdComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline GlobalVariableIdComp MakeGlobalVariableIdComp(const GlobalVariableTable& row) {
    return { row.id() };
}
