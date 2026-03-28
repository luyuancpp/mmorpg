
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/cooldown_table.pb.h"

// ============================================================
// Per-column ECS components for CooldownTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct CooldownIdComp {
    uint32_t value;
};

struct CooldownDurationComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline CooldownIdComp MakeCooldownIdComp(const CooldownTable& row) {
    return { row.id() };
}
inline CooldownDurationComp MakeCooldownDurationComp(const CooldownTable& row) {
    return { row.duration() };
}
