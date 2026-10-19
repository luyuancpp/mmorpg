
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for MysteryMerchantGoodsTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type MysteryMerchantGoodsIdComp struct {
    Value uint32
}

type MysteryMerchantGoodsNameComp struct {
    Value string
}

type MysteryMerchantGoodsItem_idComp struct {
    Value uint32
}

type MysteryMerchantGoodsItem_countComp struct {
    Value uint32
}

type MysteryMerchantGoodsCurrency_typeComp struct {
    Value uint32
}

type MysteryMerchantGoodsPriceComp struct {
    Value uint64
}

type MysteryMerchantGoodsWeightComp struct {
    Value uint32
}

type MysteryMerchantGoodsLimit_countComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeMysteryMerchantGoodsIdComp(row *pb.MysteryMerchantGoodsTable) MysteryMerchantGoodsIdComp {
    return MysteryMerchantGoodsIdComp{Value: row.Id}
}

func MakeMysteryMerchantGoodsNameComp(row *pb.MysteryMerchantGoodsTable) MysteryMerchantGoodsNameComp {
    return MysteryMerchantGoodsNameComp{Value: row.Name}
}

func MakeMysteryMerchantGoodsItem_idComp(row *pb.MysteryMerchantGoodsTable) MysteryMerchantGoodsItem_idComp {
    return MysteryMerchantGoodsItem_idComp{Value: row.ItemId}
}

func MakeMysteryMerchantGoodsItem_countComp(row *pb.MysteryMerchantGoodsTable) MysteryMerchantGoodsItem_countComp {
    return MysteryMerchantGoodsItem_countComp{Value: row.ItemCount}
}

func MakeMysteryMerchantGoodsCurrency_typeComp(row *pb.MysteryMerchantGoodsTable) MysteryMerchantGoodsCurrency_typeComp {
    return MysteryMerchantGoodsCurrency_typeComp{Value: row.CurrencyType}
}

func MakeMysteryMerchantGoodsPriceComp(row *pb.MysteryMerchantGoodsTable) MysteryMerchantGoodsPriceComp {
    return MysteryMerchantGoodsPriceComp{Value: row.Price}
}

func MakeMysteryMerchantGoodsWeightComp(row *pb.MysteryMerchantGoodsTable) MysteryMerchantGoodsWeightComp {
    return MysteryMerchantGoodsWeightComp{Value: row.Weight}
}

func MakeMysteryMerchantGoodsLimit_countComp(row *pb.MysteryMerchantGoodsTable) MysteryMerchantGoodsLimit_countComp {
    return MysteryMerchantGoodsLimit_countComp{Value: row.LimitCount}
}

