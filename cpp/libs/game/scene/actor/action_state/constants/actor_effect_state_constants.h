#pragma once
#include <cstdint>

enum eActorEffectStateType : uint32_t {
    kNormal        = 0,   // 正常状态
    kSilenced      = 1,   // 沉默状态
    kStunned       = 2    // 眩晕状态
};
