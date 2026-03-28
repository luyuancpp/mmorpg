
#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "table/proto/messagelimiter_table.pb.h"

// ============================================================
// Per-column ECS components for MessageLimiterTable
// ============================================================
// Scalar columns → value components
// String columns → std::string_view (points into proto memory)
// Repeated columns → std::span (points into proto RepeatedField)
// ============================================================


struct MessageLimiterIdComp {
    uint32_t value;
};

struct MessageLimiterMax_requestsComp {
    uint32_t value;
};

struct MessageLimiterTime_windowComp {
    uint32_t value;
};

struct MessageLimiterTip_messageComp {
    uint32_t value;
};


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

inline MessageLimiterIdComp MakeMessageLimiterIdComp(const MessageLimiterTable& row) {
    return { row.id() };
}
inline MessageLimiterMax_requestsComp MakeMessageLimiterMax_requestsComp(const MessageLimiterTable& row) {
    return { row.max_requests() };
}
inline MessageLimiterTime_windowComp MakeMessageLimiterTime_windowComp(const MessageLimiterTable& row) {
    return { row.time_window() };
}
inline MessageLimiterTip_messageComp MakeMessageLimiterTip_messageComp(const MessageLimiterTable& row) {
    return { row.tip_message() };
}
