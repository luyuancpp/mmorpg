
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/testmultikey_table.pb.h"

// ============================================================
// Per-column ECS components for TestMultiKeyTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct TestMultiKeyIdComp {
    uint32_t value;
};

struct TestMultiKeyLevelComp {
    uint32_t value;
};

struct TestMultiKeyString_keyComp {
    std::string_view value;
};

struct TestMultiKeyUint32_keyComp {
    uint32_t value;
};

struct TestMultiKeyInt32_keyComp {
    int32_t value;
};

struct TestMultiKeyM_string_keyComp {
    std::string_view value;
};

struct TestMultiKeyM_uint32_keyComp {
    uint32_t value;
};

struct TestMultiKeyM_int32_keyComp {
    int32_t value;
};

struct TestMultiKeyEffectComp {
    std::span<const uint32_t> values;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline TestMultiKeyIdComp MakeTestMultiKeyIdComp(const TestMultiKeyTable& row) {
    return { row.id() };
}
inline TestMultiKeyLevelComp MakeTestMultiKeyLevelComp(const TestMultiKeyTable& row) {
    return { row.level() };
}
inline TestMultiKeyString_keyComp MakeTestMultiKeyString_keyComp(const TestMultiKeyTable& row) {
    return { std::string_view(row.string_key()) };
}
inline TestMultiKeyUint32_keyComp MakeTestMultiKeyUint32_keyComp(const TestMultiKeyTable& row) {
    return { row.uint32_key() };
}
inline TestMultiKeyInt32_keyComp MakeTestMultiKeyInt32_keyComp(const TestMultiKeyTable& row) {
    return { row.int32_key() };
}
inline TestMultiKeyM_string_keyComp MakeTestMultiKeyM_string_keyComp(const TestMultiKeyTable& row) {
    return { std::string_view(row.m_string_key()) };
}
inline TestMultiKeyM_uint32_keyComp MakeTestMultiKeyM_uint32_keyComp(const TestMultiKeyTable& row) {
    return { row.m_uint32_key() };
}
inline TestMultiKeyM_int32_keyComp MakeTestMultiKeyM_int32_keyComp(const TestMultiKeyTable& row) {
    return { row.m_int32_key() };
}
inline TestMultiKeyEffectComp MakeTestMultiKeyEffectComp(const TestMultiKeyTable& row) {
    const auto& rf = row.effect();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
