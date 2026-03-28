
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/mainscene_table.pb.h"

// ============================================================
// Per-column ECS components for MainSceneTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct MainSceneIdComp {
    uint32_t value;
};

struct MainSceneNav_bin_fileComp {
    std::string_view value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline MainSceneIdComp MakeMainSceneIdComp(const MainSceneTable& row) {
    return { row.id() };
}
inline MainSceneNav_bin_fileComp MakeMainSceneNav_bin_fileComp(const MainSceneTable& row) {
    return { std::string_view(row.nav_bin_file()) };
}
