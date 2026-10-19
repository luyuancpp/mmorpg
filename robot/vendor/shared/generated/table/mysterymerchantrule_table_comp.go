
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for MysteryMerchantRuleTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type MysteryMerchantRuleIdComp struct {
    Value uint32
}

type MysteryMerchantRuleInterval_secondsComp struct {
    Value uint32
}

type MysteryMerchantRuleStay_secondsComp struct {
    Value uint32
}

type MysteryMerchantRuleShelf_sizeComp struct {
    Value uint32
}

type MysteryMerchantRuleWeight_discount60Comp struct {
    Value uint32
}

type MysteryMerchantRuleWeight_discount50Comp struct {
    Value uint32
}

type MysteryMerchantRuleWeight_discount40Comp struct {
    Value uint32
}

type MysteryMerchantRuleInteract_rangeComp struct {
    Value uint32
}

type MysteryMerchantRuleDescComp struct {
    Value string
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeMysteryMerchantRuleIdComp(row *pb.MysteryMerchantRuleTable) MysteryMerchantRuleIdComp {
    return MysteryMerchantRuleIdComp{Value: row.Id}
}

func MakeMysteryMerchantRuleInterval_secondsComp(row *pb.MysteryMerchantRuleTable) MysteryMerchantRuleInterval_secondsComp {
    return MysteryMerchantRuleInterval_secondsComp{Value: row.IntervalSeconds}
}

func MakeMysteryMerchantRuleStay_secondsComp(row *pb.MysteryMerchantRuleTable) MysteryMerchantRuleStay_secondsComp {
    return MysteryMerchantRuleStay_secondsComp{Value: row.StaySeconds}
}

func MakeMysteryMerchantRuleShelf_sizeComp(row *pb.MysteryMerchantRuleTable) MysteryMerchantRuleShelf_sizeComp {
    return MysteryMerchantRuleShelf_sizeComp{Value: row.ShelfSize}
}

func MakeMysteryMerchantRuleWeight_discount60Comp(row *pb.MysteryMerchantRuleTable) MysteryMerchantRuleWeight_discount60Comp {
    return MysteryMerchantRuleWeight_discount60Comp{Value: row.WeightDiscount60}
}

func MakeMysteryMerchantRuleWeight_discount50Comp(row *pb.MysteryMerchantRuleTable) MysteryMerchantRuleWeight_discount50Comp {
    return MysteryMerchantRuleWeight_discount50Comp{Value: row.WeightDiscount50}
}

func MakeMysteryMerchantRuleWeight_discount40Comp(row *pb.MysteryMerchantRuleTable) MysteryMerchantRuleWeight_discount40Comp {
    return MysteryMerchantRuleWeight_discount40Comp{Value: row.WeightDiscount40}
}

func MakeMysteryMerchantRuleInteract_rangeComp(row *pb.MysteryMerchantRuleTable) MysteryMerchantRuleInteract_rangeComp {
    return MysteryMerchantRuleInteract_rangeComp{Value: row.InteractRange}
}

func MakeMysteryMerchantRuleDescComp(row *pb.MysteryMerchantRuleTable) MysteryMerchantRuleDescComp {
    return MysteryMerchantRuleDescComp{Value: row.Desc}
}

