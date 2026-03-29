
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/mission_table.pb.h"

// ============================================================
// Per-column ECS components for MissionTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct MissionIdComp {
    uint32_t value;
};

struct MissionMission_typeComp {
    uint32_t value;
};

struct MissionMission_sub_typeComp {
    uint32_t value;
};

struct MissionCondition_orderComp {
    uint32_t value;
};

struct MissionAuto_rewardComp {
    uint32_t value;
};

struct MissionReward_idComp {
    uint32_t value;
};

struct MissionCondition_idComp {
    std::span<const uint32_t> values;
};

struct MissionNext_mission_idComp {
    std::span<const uint32_t> values;
};

struct MissionTarget_countComp {
    std::span<const uint32_t> values;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline MissionIdComp MakeMissionIdComp(const MissionTable& row) {
    return { row.id() };
}
inline MissionMission_typeComp MakeMissionMission_typeComp(const MissionTable& row) {
    return { row.mission_type() };
}
inline MissionMission_sub_typeComp MakeMissionMission_sub_typeComp(const MissionTable& row) {
    return { row.mission_sub_type() };
}
inline MissionCondition_orderComp MakeMissionCondition_orderComp(const MissionTable& row) {
    return { row.condition_order() };
}
inline MissionAuto_rewardComp MakeMissionAuto_rewardComp(const MissionTable& row) {
    return { row.auto_reward() };
}
inline MissionReward_idComp MakeMissionReward_idComp(const MissionTable& row) {
    return { row.reward_id() };
}
inline MissionCondition_idComp MakeMissionCondition_idComp(const MissionTable& row) {
    const auto& rf = row.condition_id();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
inline MissionNext_mission_idComp MakeMissionNext_mission_idComp(const MissionTable& row) {
    const auto& rf = row.next_mission_id();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
inline MissionTarget_countComp MakeMissionTarget_countComp(const MissionTable& row) {
    const auto& rf = row.target_count();
    return { std::span<const uint32_t>(rf.data(), rf.size()) };
}
