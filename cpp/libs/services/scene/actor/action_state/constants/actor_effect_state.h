#pragma once
#include <cstdint>

enum eActorEffectStateType : uint32_t {
    kNormal        = 0,   // Normal
    kSilenced      = 1,   // Silenced
    kStunned       = 2    // Stunned
};