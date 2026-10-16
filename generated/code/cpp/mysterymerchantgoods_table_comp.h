
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/mysterymerchantgoods_table.pb.h"

// ============================================================
// Per-column ECS components for MysteryMerchantGoodsTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct MysteryMerchantGoodsIdComp {
    uint32_t value;
};

struct MysteryMerchantGoodsNameComp {
    std::string_view value;
};

struct MysteryMerchantGoodsItem_idComp {
    uint32_t value;
};

struct MysteryMerchantGoodsItem_countComp {
    uint32_t value;
};

struct MysteryMerchantGoodsCurrency_typeComp {
    uint32_t value;
};

struct MysteryMerchantGoodsPriceComp {
    uint64_t value;
};

struct MysteryMerchantGoodsWeightComp {
    uint32_t value;
};

struct MysteryMerchantGoodsLimit_countComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline MysteryMerchantGoodsIdComp MakeMysteryMerchantGoodsIdComp(const MysteryMerchantGoodsTable& row) {
    return { row.id() };
}
inline MysteryMerchantGoodsNameComp MakeMysteryMerchantGoodsNameComp(const MysteryMerchantGoodsTable& row) {
    return { std::string_view(row.name()) };
}
inline MysteryMerchantGoodsItem_idComp MakeMysteryMerchantGoodsItem_idComp(const MysteryMerchantGoodsTable& row) {
    return { row.item_id() };
}
inline MysteryMerchantGoodsItem_countComp MakeMysteryMerchantGoodsItem_countComp(const MysteryMerchantGoodsTable& row) {
    return { row.item_count() };
}
inline MysteryMerchantGoodsCurrency_typeComp MakeMysteryMerchantGoodsCurrency_typeComp(const MysteryMerchantGoodsTable& row) {
    return { row.currency_type() };
}
inline MysteryMerchantGoodsPriceComp MakeMysteryMerchantGoodsPriceComp(const MysteryMerchantGoodsTable& row) {
    return { row.price() };
}
inline MysteryMerchantGoodsWeightComp MakeMysteryMerchantGoodsWeightComp(const MysteryMerchantGoodsTable& row) {
    return { row.weight() };
}
inline MysteryMerchantGoodsLimit_countComp MakeMysteryMerchantGoodsLimit_countComp(const MysteryMerchantGoodsTable& row) {
    return { row.limit_count() };
}
