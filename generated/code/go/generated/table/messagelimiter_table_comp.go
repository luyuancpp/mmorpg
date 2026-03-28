
package table

import (
    pb "game/generated/pb/table"
)

// ============================================================
// Per-column component structs for MessageLimiterTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type MessageLimiterIdComp struct {
    Value uint32
}

type MessageLimiterMax_requestsComp struct {
    Value uint32
}

type MessageLimiterTime_windowComp struct {
    Value uint32
}

type MessageLimiterTip_messageComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeMessageLimiterIdComp(row *pb.MessageLimiterTable) MessageLimiterIdComp {
    return MessageLimiterIdComp{Value: row.Id}
}

func MakeMessageLimiterMax_requestsComp(row *pb.MessageLimiterTable) MessageLimiterMax_requestsComp {
    return MessageLimiterMax_requestsComp{Value: row.MaxRequests}
}

func MakeMessageLimiterTime_windowComp(row *pb.MessageLimiterTable) MessageLimiterTime_windowComp {
    return MessageLimiterTime_windowComp{Value: row.TimeWindow}
}

func MakeMessageLimiterTip_messageComp(row *pb.MessageLimiterTable) MessageLimiterTip_messageComp {
    return MessageLimiterTip_messageComp{Value: row.TipMessage}
}

