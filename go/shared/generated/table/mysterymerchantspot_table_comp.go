
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for MysteryMerchantSpotTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type MysteryMerchantSpotIdComp struct {
    Value uint32
}

type MysteryMerchantSpotScene_config_idComp struct {
    Value uint32
}

type MysteryMerchantSpotXComp struct {
    Value float64
}

type MysteryMerchantSpotYComp struct {
    Value float64
}

type MysteryMerchantSpotZComp struct {
    Value float64
}

type MysteryMerchantSpotDescComp struct {
    Value string
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeMysteryMerchantSpotIdComp(row *pb.MysteryMerchantSpotTable) MysteryMerchantSpotIdComp {
    return MysteryMerchantSpotIdComp{Value: row.Id}
}

func MakeMysteryMerchantSpotScene_config_idComp(row *pb.MysteryMerchantSpotTable) MysteryMerchantSpotScene_config_idComp {
    return MysteryMerchantSpotScene_config_idComp{Value: row.SceneConfigId}
}

func MakeMysteryMerchantSpotXComp(row *pb.MysteryMerchantSpotTable) MysteryMerchantSpotXComp {
    return MysteryMerchantSpotXComp{Value: row.X}
}

func MakeMysteryMerchantSpotYComp(row *pb.MysteryMerchantSpotTable) MysteryMerchantSpotYComp {
    return MysteryMerchantSpotYComp{Value: row.Y}
}

func MakeMysteryMerchantSpotZComp(row *pb.MysteryMerchantSpotTable) MysteryMerchantSpotZComp {
    return MysteryMerchantSpotZComp{Value: row.Z}
}

func MakeMysteryMerchantSpotDescComp(row *pb.MysteryMerchantSpotTable) MysteryMerchantSpotDescComp {
    return MysteryMerchantSpotDescComp{Value: row.Desc}
}

