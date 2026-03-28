
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/actoractionstate_table.pb.h"

// ============================================================
// Per-column ECS components for ActorActionStateTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct ActorActionStateIdComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline ActorActionStateIdComp MakeActorActionStateIdComp(const ActorActionStateTable& row) {
    return { row.id() };
}
