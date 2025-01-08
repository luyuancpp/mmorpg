#pragma once
#include <cstdint>

enum eActorCombatState : uint32_t {
    kActorCombatStateNone,
    kActorCombatStateSilence,
    kActorMaxCombatStateType,
};


enum eCombatStateMode : uint32_t {
    kCombatStatePermitted,// 不互斥，允许执行
    kCombatStateMutualExclusion,// 互斥，禁止执行
};
