#pragma once
#include <cstdint>

enum eActorState : uint32_t {
    kCombat,           // 正在战斗
    kTeamFollow,           // 正在组队跟随
};

enum eActorAction {
    kUseSkill,        // 尝试使用技能
    kJoinFollow,      // 尝试进入组队跟随
    // 可以继续添加其他动作
};