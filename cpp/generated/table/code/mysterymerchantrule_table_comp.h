
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/mysterymerchantrule_table.pb.h"

// ============================================================
// Per-column ECS components for MysteryMerchantRuleTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct MysteryMerchantRuleIdComp {
    uint32_t value;
};

struct MysteryMerchantRuleInterval_secondsComp {
    uint32_t value;
};

struct MysteryMerchantRuleStay_secondsComp {
    uint32_t value;
};

struct MysteryMerchantRuleShelf_sizeComp {
    uint32_t value;
};

struct MysteryMerchantRuleWeight_discount60Comp {
    uint32_t value;
};

struct MysteryMerchantRuleWeight_discount50Comp {
    uint32_t value;
};

struct MysteryMerchantRuleWeight_discount40Comp {
    uint32_t value;
};

struct MysteryMerchantRuleInteract_rangeComp {
    uint32_t value;
};

struct MysteryMerchantRuleDescComp {
    std::string_view value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline MysteryMerchantRuleIdComp MakeMysteryMerchantRuleIdComp(const MysteryMerchantRuleTable& row) {
    return { row.id() };
}
inline MysteryMerchantRuleInterval_secondsComp MakeMysteryMerchantRuleInterval_secondsComp(const MysteryMerchantRuleTable& row) {
    return { row.interval_seconds() };
}
inline MysteryMerchantRuleStay_secondsComp MakeMysteryMerchantRuleStay_secondsComp(const MysteryMerchantRuleTable& row) {
    return { row.stay_seconds() };
}
inline MysteryMerchantRuleShelf_sizeComp MakeMysteryMerchantRuleShelf_sizeComp(const MysteryMerchantRuleTable& row) {
    return { row.shelf_size() };
}
inline MysteryMerchantRuleWeight_discount60Comp MakeMysteryMerchantRuleWeight_discount60Comp(const MysteryMerchantRuleTable& row) {
    return { row.weight_discount60() };
}
inline MysteryMerchantRuleWeight_discount50Comp MakeMysteryMerchantRuleWeight_discount50Comp(const MysteryMerchantRuleTable& row) {
    return { row.weight_discount50() };
}
inline MysteryMerchantRuleWeight_discount40Comp MakeMysteryMerchantRuleWeight_discount40Comp(const MysteryMerchantRuleTable& row) {
    return { row.weight_discount40() };
}
inline MysteryMerchantRuleInteract_rangeComp MakeMysteryMerchantRuleInteract_rangeComp(const MysteryMerchantRuleTable& row) {
    return { row.interact_range() };
}
inline MysteryMerchantRuleDescComp MakeMysteryMerchantRuleDescComp(const MysteryMerchantRuleTable& row) {
    return { std::string_view(row.desc()) };
}
