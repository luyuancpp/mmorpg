
package table

import (
    pb "game/generated/pb/table"
)

// ============================================================
// Per-column component structs for BuffTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type BuffIdComp struct {
    Value uint32
}

type BuffNo_casterComp struct {
    Value uint32
}

type BuffBuff_typeComp struct {
    Value uint32
}

type BuffLevelComp struct {
    Value uint32
}

type BuffMax_layerComp struct {
    Value uint32
}

type BuffInfinite_durationComp struct {
    Value uint32
}

type BuffDurationComp struct {
    Value float64
}

type BuffForce_interruptComp struct {
    Value uint32
}

type BuffIntervalComp struct {
    Value float64
}

type BuffInterval_countComp struct {
    Value uint32
}

type BuffMovement_speed_boostComp struct {
    Value float64
}

type BuffMovement_speed_reductionComp struct {
    Value float64
}

type BuffHealth_regenerationComp struct {
    Value string
}

type BuffCombat_idle_secondsComp struct {
    Value float64
}

type BuffTimeComp struct {
    Value uint32
}

type BuffBonus_damageComp struct {
    Value string
}

type BuffInterval_effectComp struct {
    Values []float64
}

type BuffSub_buffComp struct {
    Values []uint32
}

type BuffTarget_sub_buffComp struct {
    Values []uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeBuffIdComp(row *pb.BuffTable) BuffIdComp {
    return BuffIdComp{Value: row.Id}
}

func MakeBuffNo_casterComp(row *pb.BuffTable) BuffNo_casterComp {
    return BuffNo_casterComp{Value: row.NoCaster}
}

func MakeBuffBuff_typeComp(row *pb.BuffTable) BuffBuff_typeComp {
    return BuffBuff_typeComp{Value: row.BuffType}
}

func MakeBuffLevelComp(row *pb.BuffTable) BuffLevelComp {
    return BuffLevelComp{Value: row.Level}
}

func MakeBuffMax_layerComp(row *pb.BuffTable) BuffMax_layerComp {
    return BuffMax_layerComp{Value: row.MaxLayer}
}

func MakeBuffInfinite_durationComp(row *pb.BuffTable) BuffInfinite_durationComp {
    return BuffInfinite_durationComp{Value: row.InfiniteDuration}
}

func MakeBuffDurationComp(row *pb.BuffTable) BuffDurationComp {
    return BuffDurationComp{Value: row.Duration}
}

func MakeBuffForce_interruptComp(row *pb.BuffTable) BuffForce_interruptComp {
    return BuffForce_interruptComp{Value: row.ForceInterrupt}
}

func MakeBuffIntervalComp(row *pb.BuffTable) BuffIntervalComp {
    return BuffIntervalComp{Value: row.Interval}
}

func MakeBuffInterval_countComp(row *pb.BuffTable) BuffInterval_countComp {
    return BuffInterval_countComp{Value: row.IntervalCount}
}

func MakeBuffMovement_speed_boostComp(row *pb.BuffTable) BuffMovement_speed_boostComp {
    return BuffMovement_speed_boostComp{Value: row.MovementSpeedBoost}
}

func MakeBuffMovement_speed_reductionComp(row *pb.BuffTable) BuffMovement_speed_reductionComp {
    return BuffMovement_speed_reductionComp{Value: row.MovementSpeedReduction}
}

func MakeBuffHealth_regenerationComp(row *pb.BuffTable) BuffHealth_regenerationComp {
    return BuffHealth_regenerationComp{Value: row.HealthRegeneration}
}

func MakeBuffCombat_idle_secondsComp(row *pb.BuffTable) BuffCombat_idle_secondsComp {
    return BuffCombat_idle_secondsComp{Value: row.CombatIdleSeconds}
}

func MakeBuffTimeComp(row *pb.BuffTable) BuffTimeComp {
    return BuffTimeComp{Value: row.Time}
}

func MakeBuffBonus_damageComp(row *pb.BuffTable) BuffBonus_damageComp {
    return BuffBonus_damageComp{Value: row.BonusDamage}
}

func MakeBuffInterval_effectComp(row *pb.BuffTable) BuffInterval_effectComp {
    return BuffInterval_effectComp{Values: row.IntervalEffect}
}

func MakeBuffSub_buffComp(row *pb.BuffTable) BuffSub_buffComp {
    return BuffSub_buffComp{Values: row.SubBuff}
}

func MakeBuffTarget_sub_buffComp(row *pb.BuffTable) BuffTarget_sub_buffComp {
    return BuffTarget_sub_buffComp{Values: row.TargetSubBuff}
}

