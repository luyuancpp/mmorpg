
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/actoractioncombatstate_table.pb.h"

// ============================================================
// Per-column ECS components for ActorActionCombatStateTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct ActorActionCombatStateIdComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline ActorActionCombatStateIdComp MakeActorActionCombatStateIdComp(const ActorActionCombatStateTable& row) {
    return { row.id() };
}
