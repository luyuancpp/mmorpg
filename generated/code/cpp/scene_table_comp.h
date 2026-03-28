
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/scene_table.pb.h"

// ============================================================
// Per-column ECS components for SceneTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct SceneIdComp {
    uint32_t value;
};

struct SceneDungeon_idComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline SceneIdComp MakeSceneIdComp(const SceneTable& row) {
    return { row.id() };
}
inline SceneDungeon_idComp MakeSceneDungeon_idComp(const SceneTable& row) {
    return { row.dungeon_id() };
}
