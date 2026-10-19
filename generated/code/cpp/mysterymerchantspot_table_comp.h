
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/mysterymerchantspot_table.pb.h"

// ============================================================
// Per-column ECS components for MysteryMerchantSpotTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct MysteryMerchantSpotIdComp {
    uint32_t value;
};

struct MysteryMerchantSpotScene_config_idComp {
    uint32_t value;
};

struct MysteryMerchantSpotXComp {
    double value;
};

struct MysteryMerchantSpotYComp {
    double value;
};

struct MysteryMerchantSpotZComp {
    double value;
};

struct MysteryMerchantSpotDescComp {
    std::string_view value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline MysteryMerchantSpotIdComp MakeMysteryMerchantSpotIdComp(const MysteryMerchantSpotTable& row) {
    return { row.id() };
}
inline MysteryMerchantSpotScene_config_idComp MakeMysteryMerchantSpotScene_config_idComp(const MysteryMerchantSpotTable& row) {
    return { row.scene_config_id() };
}
inline MysteryMerchantSpotXComp MakeMysteryMerchantSpotXComp(const MysteryMerchantSpotTable& row) {
    return { row.x() };
}
inline MysteryMerchantSpotYComp MakeMysteryMerchantSpotYComp(const MysteryMerchantSpotTable& row) {
    return { row.y() };
}
inline MysteryMerchantSpotZComp MakeMysteryMerchantSpotZComp(const MysteryMerchantSpotTable& row) {
    return { row.z() };
}
inline MysteryMerchantSpotDescComp MakeMysteryMerchantSpotDescComp(const MysteryMerchantSpotTable& row) {
    return { std::string_view(row.desc()) };
}
