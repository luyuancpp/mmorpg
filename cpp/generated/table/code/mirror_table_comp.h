
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/mirror_table.pb.h"

// ============================================================
// Per-column ECS components for MirrorTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct MirrorIdComp {
    uint32_t value;
};

struct MirrorScene_idComp {
    uint32_t value;
};

struct MirrorMain_scene_idComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline MirrorIdComp MakeMirrorIdComp(const MirrorTable& row) {
    return { row.id() };
}
inline MirrorScene_idComp MakeMirrorScene_idComp(const MirrorTable& row) {
    return { row.scene_id() };
}
inline MirrorMain_scene_idComp MakeMirrorMain_scene_idComp(const MirrorTable& row) {
    return { row.main_scene_id() };
}
