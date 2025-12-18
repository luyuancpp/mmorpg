#pragma once
#include <cstdint>

using InstanceId = uint64_t;

constexpr InstanceId kGlobalInstanceId = 0; // 约定：0 表示全局/永久模板（一次性）
