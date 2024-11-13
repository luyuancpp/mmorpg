#pragma once
#include <cstdint>

enum eActorState : uint32_t {
    kCombat,           // 正在战斗
    kTeamFollow,           // 正在组队跟随
    kActorStateMax,
};

enum eActorAction {
    kUseSkill,        // 尝试使用技能
    kJoinFollow,      // 尝试进入组队跟随
    // 可以继续添加其他动作
};

enum eActionStateFlag
{
    kMutualExclusion,// 互斥，禁止执行
    kAllow,// 不互斥，允许执行
    kInterruptAndExecute,// 打断并执行
};
