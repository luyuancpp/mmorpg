
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/test_table.pb.h"

// ============================================================
// Per-column ECS components for TestTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct TestIdComp {
    uint32_t value;
};

struct TestLevelComp {
    uint32_t value;
};

struct TestEffectComp {
    std::span<const uint32_t> values;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline TestIdComp MakeTestIdComp(const TestTable& row) {
    return { row.id() };
}
inline TestLevelComp MakeTestLevelComp(const TestTable& row) {
    return { row.level() };
}
inline TestEffectComp MakeTestEffectComp(const TestTable& row) {
    const auto& rf = row.effect();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
