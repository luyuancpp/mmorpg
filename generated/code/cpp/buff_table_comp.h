
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/buff_table.pb.h"

// ============================================================
// Per-column ECS components for BuffTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct BuffIdComp {
    uint32_t value;
};

struct BuffNo_casterComp {
    uint32_t value;
};

struct BuffBuff_typeComp {
    uint32_t value;
};

struct BuffLevelComp {
    uint32_t value;
};

struct BuffMax_layerComp {
    uint32_t value;
};

struct BuffInfinite_durationComp {
    uint32_t value;
};

struct BuffDurationComp {
    double value;
};

struct BuffForce_interruptComp {
    uint32_t value;
};

struct BuffIntervalComp {
    double value;
};

struct BuffInterval_countComp {
    uint32_t value;
};

struct BuffMovement_speed_boostComp {
    double value;
};

struct BuffMovement_speed_reductionComp {
    double value;
};

struct BuffHealth_regenerationComp {
    std::string_view value;
};

struct BuffCombat_idle_secondsComp {
    double value;
};

struct BuffTimeComp {
    uint32_t value;
};

struct BuffBonus_damageComp {
    std::string_view value;
};

struct BuffInterval_effectComp {
    std::span<const double> values;
};

struct BuffSub_buffComp {
    std::span<const uint32_t> values;
};

struct BuffTarget_sub_buffComp {
    std::span<const uint32_t> values;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline BuffIdComp MakeBuffIdComp(const BuffTable& row) {
    return { row.id() };
}
inline BuffNo_casterComp MakeBuffNo_casterComp(const BuffTable& row) {
    return { row.no_caster() };
}
inline BuffBuff_typeComp MakeBuffBuff_typeComp(const BuffTable& row) {
    return { row.buff_type() };
}
inline BuffLevelComp MakeBuffLevelComp(const BuffTable& row) {
    return { row.level() };
}
inline BuffMax_layerComp MakeBuffMax_layerComp(const BuffTable& row) {
    return { row.max_layer() };
}
inline BuffInfinite_durationComp MakeBuffInfinite_durationComp(const BuffTable& row) {
    return { row.infinite_duration() };
}
inline BuffDurationComp MakeBuffDurationComp(const BuffTable& row) {
    return { row.duration() };
}
inline BuffForce_interruptComp MakeBuffForce_interruptComp(const BuffTable& row) {
    return { row.force_interrupt() };
}
inline BuffIntervalComp MakeBuffIntervalComp(const BuffTable& row) {
    return { row.interval() };
}
inline BuffInterval_countComp MakeBuffInterval_countComp(const BuffTable& row) {
    return { row.interval_count() };
}
inline BuffMovement_speed_boostComp MakeBuffMovement_speed_boostComp(const BuffTable& row) {
    return { row.movement_speed_boost() };
}
inline BuffMovement_speed_reductionComp MakeBuffMovement_speed_reductionComp(const BuffTable& row) {
    return { row.movement_speed_reduction() };
}
inline BuffHealth_regenerationComp MakeBuffHealth_regenerationComp(const BuffTable& row) {
    return { std::string_view(row.health_regeneration()) };
}
inline BuffCombat_idle_secondsComp MakeBuffCombat_idle_secondsComp(const BuffTable& row) {
    return { row.combat_idle_seconds() };
}
inline BuffTimeComp MakeBuffTimeComp(const BuffTable& row) {
    return { row.time() };
}
inline BuffBonus_damageComp MakeBuffBonus_damageComp(const BuffTable& row) {
    return { std::string_view(row.bonus_damage()) };
}
inline BuffInterval_effectComp MakeBuffInterval_effectComp(const BuffTable& row) {
    const auto& rf = row.interval_effect();
    return { std::span<const double>(rf.data(), rf.size()) };
}
inline BuffSub_buffComp MakeBuffSub_buffComp(const BuffTable& row) {
    const auto& rf = row.sub_buff();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
inline BuffTarget_sub_buffComp MakeBuffTarget_sub_buffComp(const BuffTable& row) {
    const auto& rf = row.target_sub_buff();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
