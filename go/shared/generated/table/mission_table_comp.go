
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for MissionTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type MissionIdComp struct {
    Value uint32
}

type MissionMission_typeComp struct {
    Value uint32
}

type MissionMission_sub_typeComp struct {
    Value uint32
}

type MissionCondition_orderComp struct {
    Value uint32
}

type MissionAuto_rewardComp struct {
    Value uint32
}

type MissionReward_idComp struct {
    Value uint32
}

type MissionCondition_idComp struct {
    Values []uint32
}

type MissionNext_mission_idComp struct {
    Values []uint32
}

type MissionTarget_countComp struct {
    Values []uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeMissionIdComp(row *pb.MissionTable) MissionIdComp {
    return MissionIdComp{Value: row.Id}
}

func MakeMissionMission_typeComp(row *pb.MissionTable) MissionMission_typeComp {
    return MissionMission_typeComp{Value: row.MissionType}
}

func MakeMissionMission_sub_typeComp(row *pb.MissionTable) MissionMission_sub_typeComp {
    return MissionMission_sub_typeComp{Value: row.MissionSubType}
}

func MakeMissionCondition_orderComp(row *pb.MissionTable) MissionCondition_orderComp {
    return MissionCondition_orderComp{Value: row.ConditionOrder}
}

func MakeMissionAuto_rewardComp(row *pb.MissionTable) MissionAuto_rewardComp {
    return MissionAuto_rewardComp{Value: row.AutoReward}
}

func MakeMissionReward_idComp(row *pb.MissionTable) MissionReward_idComp {
    return MissionReward_idComp{Value: row.RewardId}
}

func MakeMissionCondition_idComp(row *pb.MissionTable) MissionCondition_idComp {
    return MissionCondition_idComp{Values: row.ConditionId}
}

func MakeMissionNext_mission_idComp(row *pb.MissionTable) MissionNext_mission_idComp {
    return MissionNext_mission_idComp{Values: row.NextMissionId}
}

func MakeMissionTarget_countComp(row *pb.MissionTable) MissionTarget_countComp {
    return MissionTarget_countComp{Values: row.TargetCount}
}

